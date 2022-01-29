#include "helper.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "router_globals.h"

esp_err_t downloadStatic(httpd_req_t *req, const char *fileStart, const size_t fileSize)
{
    httpd_resp_set_hdr(req, "Cache-Control", "max-age=31536000");

    setCloseHeader(req);

    return httpd_resp_send(req, fileStart, fileSize);
}

void setCloseHeader(httpd_req_t *req)
{
    const char *field = "Connection";
    const char *value = "close";
    httpd_resp_set_hdr(req, field, value);
}

void fillParamArray(char *buf, char *argv[], char *ssidKey, char *passKey)
{
    char ssidParam[32];
    char passParam[64];

    if (httpd_query_key_value(buf, ssidKey, ssidParam, 32) == ESP_OK)
    {
        preprocess_string(ssidParam);
        ESP_LOGI(TAG, "Found URL query parameter => %s=%s", ssidKey, ssidParam);

        httpd_query_key_value(buf, passKey, passParam, 64);
        preprocess_string(passParam);
        ESP_LOGI(TAG, "Found URL query parameter => %s=%s", passKey, passParam);
        strcpy(argv[1], ssidParam);
        strcpy(argv[2], passParam);
    }
}

void setApByQuery(char *buf, nvs_handle_t nvs)
{
    int argc = 3;
    char *argv[argc];
    argv[0] = "set_ap";
    fillParamArray(buf, argv, "ap_ssid", "ap_password");
    nvs_set_str(nvs, "ap_ssid", argv[1]);
    nvs_set_str(nvs, "ap_passwd", argv[2]);
}

char *findTextColorForSSID(int8_t rssi)
{
    char *color;
    if (rssi >= -50)
    {
        color = "success";
    }
    else if (rssi >= -70)
    {
        color = "info";
    }
    else
    {
        color = "warning";
    }
    return color;
}

void setStaByQuery(char *buf, nvs_handle_t nvs)
{
    int argc = 3;
    char *argv[argc];
    argv[0] = "set_sta";
    fillParamArray(buf, argv, "ssid", "password");
    nvs_set_str(nvs, "ssid", argv[1]);
    nvs_set_str(nvs, "passwd", argv[2]);
}

static char BALLOT_BOX[] = "&#9744;";
static char BALLOT_BOX_WITH_CHECK[] = "&#9745;";

void fillInfoData(char **clients, char **db, char **symbol, char **textColor)
{

    *clients = "0";
    *db = malloc(5);
    sprintf(*db, "%d", 0);
    *symbol = BALLOT_BOX;
    *textColor = "info";
    wifi_ap_record_t apinfo;
    memset(&apinfo, 0, sizeof(apinfo));
    if (esp_wifi_sta_get_ap_info(&apinfo) == ESP_OK)
    {
        *db = malloc(5);
        sprintf(*db, "%d", apinfo.rssi);
        *symbol = BALLOT_BOX_WITH_CHECK;
        *textColor = findTextColorForSSID(apinfo.rssi);
        ESP_LOGI(TAG, "RSSI: %d", apinfo.rssi);
        ESP_LOGI(TAG, "Channel: %d", apinfo.primary);
        ESP_LOGI(TAG, "SSID: %s", apinfo.ssid);
    };
    *clients = malloc(6);
    sprintf(*clients, "%i", connect_count);
}