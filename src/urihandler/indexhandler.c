#include "handler.h"
#include <sys/param.h>
#include "router_globals.h"

static const char *TAG = "IndexHandler";

char *appliedSSID = NULL;

bool isWrongHost(httpd_req_t *req)
{
    char *currentIP = getDefaultIPByNetmask();
    size_t buf_len = strlen(currentIP) + 1;
    char *host = malloc(buf_len);
    httpd_req_get_hdr_value_str(req, "Host", host, buf_len);
    bool out = strcmp(host, currentIP) != 0;
    free(host);
    return out;
}

esp_err_t index_get_handler(httpd_req_t *req)
{
    if (isWrongHost(req) && isDnsStarted())
    {
        ESP_LOGI(TAG, "Captive portal redirect");
        return redirectToRoot(req);
    }

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
    char *db = NULL;
    char *symbol = NULL;
    char *textColor = NULL;
    fillInfoData(&db, &symbol, &textColor);

    size = size + strlen(symbol) + strlen(textColor) + 5 /* Länge der clients */ + strlen(db);
    /* WPA2  */
    char *wpa2CB = NULL;
    char *wpa2Input = NULL;
    char *sta_identity = NULL;
    char *sta_user = NULL;
    get_config_param_str("sta_identity", &sta_identity);
    get_config_param_str("sta_user", &sta_user);

    if ((sta_identity != NULL && strlen(sta_identity) != 0) || (sta_user != NULL && strlen(sta_user) != 0))
    {
        wpa2CB = "checked";
        wpa2Input = "block";
        if (sta_identity == NULL)
        {
            sta_identity = "";
        }
        if (sta_user == NULL)
        {
            sta_user = "";
        }
    }
    else
    {
        wpa2CB = "";
        wpa2Input = "none";
        sta_identity = "";
        sta_user = "";
    }

    size = size + strlen(wpa2CB) + strlen(wpa2Input) + strlen(sta_identity) + strlen(sta_user);
    ESP_LOGI(TAG, "Allocating additional %d bytes for config page.", config_html_size + size);

    char *config_page = malloc(config_html_size + size);
    uint16_t connect_count = getConnectCount();

    if (appliedSSID != NULL && strlen(appliedSSID) > 0)
    {
        sprintf(config_page, config_start, connect_count, ap_ssid, ap_passwd, textColor, symbol, db, wpa2CB, appliedSSID, wpa2Input, sta_identity, sta_user, "", display);
    }
    else
    {
        sprintf(config_page, config_start, connect_count, ap_ssid, ap_passwd, textColor, symbol, db, wpa2CB, ssid, wpa2Input, sta_identity, sta_user, passwd, display);
    }

    closeHeader(req);

    esp_err_t ret = httpd_resp_send(req, config_page, HTTPD_RESP_USE_STRLEN);
    free(config_page);
    free(appliedSSID);
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
