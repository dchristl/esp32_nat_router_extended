#include "handler.h"

static const char *TAG_HANDLER = "LockHandler";

void closeHeader(httpd_req_t *req)
{
    httpd_resp_set_hdr(req, "Connection", "close");
}

esp_err_t download(httpd_req_t *req, const char *fileStart)
{
    httpd_resp_set_hdr(req, "Cache-Control", "max-age=31536000");
    closeHeader(req);
    return httpd_resp_send(req, fileStart, HTTPD_RESP_USE_STRLEN);
}

esp_err_t styles_download_get_handler(httpd_req_t *req)
{
    extern const unsigned char styles_start[] asm("_binary_styles_67aa3b0203355627b525be2ea57be7bf_css_start");
    httpd_resp_set_type(req, "text/css");
    ESP_LOGD(TAG_HANDLER, "Requesting style");
    return download(req, (const char *)styles_start);
}

esp_err_t jquery_get_handler(httpd_req_t *req)
{
    extern const unsigned char jquery_js_start[] asm("_binary_jquery_8a1045d9cbf50b52a0805c111ba08e94_js_start");
    httpd_resp_set_type(req, "text/javascript");
    ESP_LOGD(TAG_HANDLER, "Requesting jquery");
    return download(req, (const char *)jquery_js_start);
}

// Handler to download a "favicon.ico" file kept on the server
esp_err_t favicon_get_handler(httpd_req_t *req)
{
    extern const char favicon_ico_start[] asm("_binary_favicon_ico_start");
    extern const char favicon_ico_end[] asm("_binary_favicon_ico_end");
    const size_t favicon_ico_size = (favicon_ico_end - favicon_ico_start);
    httpd_resp_set_type(req, "image/x-icon");
    ESP_LOGD(TAG_HANDLER, "Requesting favicon");
    closeHeader(req);
    return httpd_resp_send(req, favicon_ico_start, favicon_ico_size);
}

esp_err_t redirectToRoot(httpd_req_t *req)
{
    httpd_resp_set_status(req, "302 Temporary Redirect");
    char *currentIP = getDefaultIPByNetmask();
    char str[strlen("http://") + strlen(currentIP)];
    strcpy(str, "http://");
    strcat(str, currentIP);
    httpd_resp_set_hdr(req, "Location", str);
    httpd_resp_set_hdr(req, "Connection", "Close");
    httpd_resp_send(req, "", HTTPD_RESP_USE_STRLEN);
    free(currentIP);

    return ESP_OK;
}

esp_err_t http_404_error_handler(httpd_req_t *req, httpd_err_code_t err)
{
    httpd_resp_set_status(req, "302 Temporary Redirect");
    httpd_resp_set_hdr(req, "Location", "/");
    return httpd_resp_send(req, NULL, 0);
}

esp_err_t reset_get_handler(httpd_req_t *req)
{
    httpd_req_to_sockfd(req);
    extern const char reset_start[] asm("_binary_reset_html_start");

    closeHeader(req);

    esp_err_t ret = httpd_resp_send(req, reset_start, HTTPD_RESP_USE_STRLEN);
    return ret;
}