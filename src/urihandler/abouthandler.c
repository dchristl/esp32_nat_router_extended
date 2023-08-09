#include "handler.h"

static const char *TAG = "AboutHandler";

esp_err_t about_get_handler(httpd_req_t *req)
{

    ESP_LOGI(TAG, "Requesting about page");
    if (isLocked())
    {
        return redirectToLock(req);
    }
    httpd_req_to_sockfd(req);

    extern const char about_start[] asm("_binary_about_html_start");
    extern const char about_end[] asm("_binary_about_html_end");
    const size_t about_html_size = (about_end - about_start);

    char *about_page = malloc(about_html_size + strlen(GLOBAL_VERSION) + strlen(GLOBAL_HASH) + strlen(GLOBAL_BUILD_DATE) + 1);

    sprintf(about_page, about_start, GLOBAL_VERSION, GLOBAL_HASH, GLOBAL_BUILD_DATE);

    closeHeader(req);

    esp_err_t out = httpd_resp_send(req, about_page, HTTPD_RESP_USE_STRLEN);
    free(about_page);
    return out;
}