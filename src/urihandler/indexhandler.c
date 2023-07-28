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
        return redirectToLock(req);
    }
    char *result_param = NULL;
    char *displayResult = "none";

    char *scanButtonWidth = "12";
    get_config_param_str("scan_result", &result_param);
    int32_t result_shown = 0;
    get_config_param_int("result_shown", &result_shown);
    if (result_param != NULL && result_shown < 3)
    {
        if (result_shown == 0)
        {
            free(result_param);
            ESP_LOGI(TAG, "Scan result is available and not shown already. Forwarding to scan page");
            return result_download_get_handler(req);
        }

        scanButtonWidth = "9";
        displayResult = "block";
    }

    httpd_req_to_sockfd(req);
    extern const char config_start[] asm("_binary_config_html_start");
    extern const char config_end[] asm("_binary_config_html_end");
    const size_t config_html_size = (config_end - config_start);

    char *displayLockButton = NULL;
    char *displayRelockButton = NULL;

    char *lock_pass = NULL;
    get_config_param_str("lock_pass", &lock_pass);
    if (lock_pass != NULL && strlen(lock_pass) > 0)
    {
        displayLockButton = "none";
        displayRelockButton = "flex";
    }
    else
    {
        displayLockButton = "block";
        displayRelockButton = "none";
    }

    int32_t ssidHidden = 0;
    char *hiddenSSID = NULL;
    get_config_param_int("ssid_hidden", &ssidHidden);
    if (ssidHidden == 1)
    {
        hiddenSSID = "checked";
    }
    else
    {
        hiddenSSID = "";
    }

    size_t size = strlen(ap_ssid) + strlen(ap_passwd) + strlen(displayLockButton);
    if (appliedSSID != NULL && strlen(appliedSSID) > 0)
    {
        size = size + strlen(appliedSSID);
    }
    else
    {
        size = size + strlen(ssid) + strlen(passwd);
    }
    char *db = NULL;
    char *textColor = NULL;
    char *wifiOn, *wifiOff = NULL;
    fillInfoData(&db, &textColor);
    if (strcmp(db, "0") == 0)
    {
        wifiOn = "none";
        wifiOff = "inline-block";
    }
    else
    {
        wifiOn = "inline-block";
        wifiOff = "none";
    }

    size = size + +strlen(wifiOn) + strlen(wifiOff) + strlen(textColor) + 5 /* Länge der clients */ + strlen(db);
    /* WPA2  */
    char *wpa2CB = NULL;
    char *wpa2Input = NULL;
    char *sta_identity = NULL;
    char *sta_user = NULL;
    size_t len = 0;

    char *cert = NULL;
    get_config_param_str("sta_identity", &sta_identity);
    get_config_param_str("sta_user", &sta_user);

    get_config_param_blob("cer", &cert, &len);
    char *cer = NULL;
    if (len > 0)
    {
        cer = (char *)malloc(len + 1 * sizeof(char));
        strncpy(cer, cert, len + 1);
    }
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
        if (cer == NULL)
        {
            cer = "";
        }
    }
    else
    {
        wpa2CB = "";
        wpa2Input = "none";
        sta_identity = "";
        sta_user = "";
        cer = "";
    }

    size = size + strlen(wpa2CB) + strlen(wpa2Input) + strlen(sta_identity) + strlen(sta_user) + strlen(cer) + strlen(displayResult) + strlen(scanButtonWidth) + strlen(displayLockButton) + strlen(displayRelockButton);
    ESP_LOGI(TAG, "Allocating additional %d bytes for config page.", config_html_size + size);

    char *config_page = malloc(config_html_size + size);
    uint16_t connect_count = getConnectCount();

    if (appliedSSID != NULL && strlen(appliedSSID) > 0)
    {
        sprintf(config_page, config_start, connect_count, hiddenSSID, ap_ssid, ap_passwd, textColor, wifiOff, wifiOn, db, wpa2CB, appliedSSID, wpa2Input, sta_identity, sta_user, cer, "", scanButtonWidth, displayResult, displayLockButton, displayRelockButton);
    }
    else
    {
        sprintf(config_page, config_start, connect_count, hiddenSSID, ap_ssid, ap_passwd, textColor, wifiOff, wifiOn, db, wpa2CB, ssid, wpa2Input, sta_identity, sta_user, cer, passwd, scanButtonWidth, displayResult, displayLockButton, displayRelockButton);
    }

    closeHeader(req);

    esp_err_t ret = httpd_resp_send(req, config_page, HTTPD_RESP_USE_STRLEN);
    free(config_page);
    free(appliedSSID);
    free(db);
    if (strlen(cer) > 0) // Error on C3
    {
        free(cer);
    }

    return ret;
}

esp_err_t index_post_handler(httpd_req_t *req)
{

    if (isLocked())
    {
        return redirectToLock(req);
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
    }
    char ssidParam[req->content_len];
    readUrlParameterIntoBuffer(buf, "ssid", ssidParam, req->content_len);

    if (strlen(ssidParam) > 0)
    {
        ESP_LOGI(TAG, "Found SSID parameter => %s", ssidParam);
        appliedSSID = malloc(strlen(ssidParam) + 1);
        strcpy(appliedSSID, ssidParam);
    }

    return index_get_handler(req);
}
