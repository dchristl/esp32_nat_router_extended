#include "handler.h"
#include <sys/param.h>
#include "router_globals.h"
static const char *TAG = "Advancedhandler";

esp_err_t advanced_download_get_handler(httpd_req_t *req)
{
    if (isLocked())
    {
        return unlock_handler(req);
    }

    httpd_req_to_sockfd(req);
    extern const char advanced_start[] asm("_binary_advanced_html_start");
    extern const char advanced_end[] asm("_binary_advanced_html_end");
    const size_t advanced_html_size = (advanced_end - advanced_start);

    // char *display = NULL;

    int param_count = 2;

    int keepAlive = 0;
    int ledDisabled = 0;
    char *aliveCB = "", *ledCB = "";
    size_t size = param_count * 2; //%s for parameter substitution

    get_config_param_int("keep_alive", &keepAlive);
    if (keepAlive == 1)
    {
        aliveCB = "checked";
    }
    get_config_param_int("led_disabled", &ledDisabled);
    if (ledDisabled == 0)
    {
        ledCB = "checked";
    }

    size = size + strlen(aliveCB) + strlen(ledCB); // + strlen(clients) + strlen(db);
    ESP_LOGI(TAG, "Allocating additional %d bytes for config page.", advanced_html_size + size);
    char *advanced_page = malloc(advanced_html_size + size);
    ESP_LOGI(TAG, "1");
    sprintf(advanced_page, advanced_start, ledCB, aliveCB);
    ESP_LOGI(TAG, "2");
    closeHeader(req);
    ESP_LOGI(TAG, "3");
    esp_err_t ret = httpd_resp_send(req, advanced_page, strlen(advanced_page) - (param_count * 2)); // -2 for every parameter substitution (%s)
    ESP_LOGI(TAG, "4");
    free(advanced_page);
    free(aliveCB);
    free(ledCB);
    // free(db);
    return ret;
}
