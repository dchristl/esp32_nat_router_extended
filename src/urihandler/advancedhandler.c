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
    // char *result_param = NULL;
    // get_config_param_str("scan_result", &result_param);
    // if (result_param != NULL)
    // {
    //     free(result_param);
    //     ESP_LOGI(TAG, "Scan result is available. Forwarding to scan page");
    //     return result_download_get_handler(req);
    // }

    httpd_req_to_sockfd(req);
    extern const char advanced_start[] asm("_binary_advanced_html_start");
    extern const char advanced_end[] asm("_binary_advanced_html_end");
    const size_t advanced_html_size = (advanced_end - advanced_start);

    // char *display = NULL;

    // char *lock_pass = NULL;
    // get_config_param_str("lock_pass", &lock_pass);
    // if (lock_pass != NULL && strlen(lock_pass) > 0)
    // {
    //     display = "block";
    // }
    // else
    // {
    //     display = "none";
    // }
    size_t size = 0;
    // size_t size = strlen(ap_ssid) + strlen(ap_passwd) + strlen(display);
    // if (appliedSSID != NULL && strlen(appliedSSID) > 0)
    // {
    //     size = size + strlen(appliedSSID);
    // }
    // else
    // {
    //     size = size + strlen(ssid) + strlen(passwd);
    // }
    // char *clients = malloc(6);
    // char *db = malloc(5);
    // char *symbol = NULL;
    // char *textColor = NULL;
    // fillInfoData(&clients, &db, &symbol, &textColor);

    // size = size + strlen(symbol) + strlen(textColor) + strlen(clients) + strlen(db);
    ESP_LOGI(TAG, "Allocating additional %d bytes for config page.", advanced_html_size + size);
    char *advanced_page = malloc(advanced_html_size + 1);
    sprintf(advanced_page, advanced_start, "");

    // if (appliedSSID != NULL && strlen(appliedSSID) > 0)
    // {
    //     sprintf(config_page, config_start, clients, ap_ssid, ap_passwd, textColor, symbol, db, appliedSSID, "", display);
    // }
    // else
    // {
    //     sprintf(config_page, config_start, clients, ap_ssid, ap_passwd, textColor, symbol, db, ssid, passwd, display);
    // }

    closeHeader(req);

    esp_err_t ret = httpd_resp_send(req, advanced_page, advanced_html_size); // 9 *2 for parameter substitution (%s)
    free(advanced_page);
    // free(appliedSSID);
    // free(clients);
    // free(db);
    return ret;
}
