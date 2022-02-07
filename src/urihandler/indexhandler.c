#include "handler.h"

#include <sys/param.h>

#include "router_globals.h"

static const char *TAG = "IndexHandler";

char *appliedSSID = NULL;


esp_err_t index_get_handler(httpd_req_t *req)
{
    if (isLocked())
    {
        return unlock_handler(req);
    }

    char *result_param = NULL;
    get_config_param_str("scan_result", &result_param);
    if (result_param != NULL)
    {
        free(result_param);
        ESP_LOGI(TAG, "Scan result is available. Forwarding to scan page");
        return result_download_get_handler(req); 
    }

    httpd_req_to_sockfd(req);
    extern const char config_start[] asm("_binary_config_html_start");
    extern const char config_end[] asm("_binary_config_html_end");
    const size_t config_html_size = (config_end - config_start);

    char *display = NULL;

    char *lock_pass = NULL;
    get_config_param_str("lock_pass", &lock_pass);
    if (lock_pass != NULL && strlen(lock_pass) > 0)
    {
        display = "block";
    }
    else
    {
        display = "none";
    }

    size_t size = strlen(ap_ssid) + strlen(ap_passwd) + strlen(display);
    if (appliedSSID != NULL && strlen(appliedSSID) > 0)
    {
        size = size + strlen(appliedSSID);
    }
    else
    {
        size = size + strlen(ssid) + strlen(passwd);
    }
    char *clients = malloc(6);
    char *db = malloc(5);
    char *symbol = NULL;
    char *textColor = NULL;
    fillInfoData(&clients, &db, &symbol, &textColor);

    size = size + strlen(symbol) + strlen(textColor) + strlen(clients) + strlen(db);
    ESP_LOGI(TAG, "Allocating additional %d bytes for config page.", config_html_size + size);
    char *config_page = malloc(config_html_size + size);

    if (appliedSSID != NULL && strlen(appliedSSID) > 0)
    {
        sprintf(config_page, config_start, clients, ap_ssid, ap_passwd, textColor, symbol, db, appliedSSID, "", display);
    }
    else
    {
        sprintf(config_page, config_start, clients, ap_ssid, ap_passwd, textColor, symbol, db, ssid, passwd, display);
    }

    closeHeader(req);

    esp_err_t ret = httpd_resp_send(req, config_page, config_html_size + size - 18); // 9 *2 for parameter substitution (%s)
    free(config_page);
    free(appliedSSID);
    free(clients);
    free(db);
    return ret;
}


esp_err_t index_post_handler(httpd_req_t *req)
{

    if (isLocked())
    {
        return unlock_handler(req);
    }
    httpd_req_to_sockfd(req);

    int ret, remaining = req->content_len;
    char buf[req->content_len];

    while (remaining > 0)
    {
        /* Read the data for the request */
        if ((ret = httpd_req_recv(req, buf, MIN(remaining, sizeof(buf)))) <= 0)
        {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT)
            {
                continue;
            }
            ESP_LOGE(TAG, "Timeout occured");
            return ESP_FAIL;
        }

        remaining -= ret;
        ESP_LOGI(TAG, "Found parameter query => %s", buf);
        char ssidParam[req->content_len];
        if (httpd_query_key_value(buf, "ssid", ssidParam, sizeof(ssidParam)) == ESP_OK)
        {
            preprocess_string(ssidParam);
            ESP_LOGI(TAG, "Found SSID parameter => %s (%d)", ssidParam, strlen(ssidParam));
            if (strlen(ssidParam) > 0)
            {
                appliedSSID = malloc(strlen(ssidParam) + 1);
                strcpy(appliedSSID, ssidParam);
            }
        }
    }

    return index_get_handler(req);
}
