#include "handler.h"

#include <esp_wifi.h>

#include "nvs.h"
#include "cmd_nvs.h"
#include "router_globals.h"

static const char *TAG = "ResultHandler";

const char *ROW_TEMPLATE = "<tr class='text-%s'><td>%s</td><td>%s</td><td><form action='/' method='POST'><input type='hidden' name='ssid' value='%s'><input type='submit' value='Use' name='use' class='btn btn-primary'/></form></td></tr>";

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


esp_err_t result_download_get_handler(httpd_req_t *req)
{
    if (isLocked())
    {
        return unlock_handler(req);
    }

    httpd_req_to_sockfd(req);

    extern const char result_start[] asm("_binary_result_html_start");
    extern const char result_end[] asm("_binary_result_html_end");
    const size_t result_html_size = (result_end - result_start);

    char *result_param = NULL;
    int allocatedSize = (strlen(ROW_TEMPLATE) + 100) * DEFAULT_SCAN_LIST_SIZE;

    char result[allocatedSize];
    strcpy(result, "");
    get_config_param_str("scan_result", &result_param);
    if (result_param == NULL)
    {
        strcat(result, "<tr class='text-danger'><td colspan='3'>No networks found</td></tr>");
    }
    else
    {
        char *end_str;
        char *row = strtok_r(result_param, "\x05", &end_str);
        while (row != NULL)
        {
            char *template = malloc(strlen(ROW_TEMPLATE) + 100);
            char *ssid = strtok(row, "\x03");
            char *rssi = strtok(NULL, "\x03");

            char *css = findTextColorForSSID(atoi(rssi));
            sprintf(template, ROW_TEMPLATE, css, ssid, rssi, ssid);
            strcat(result, template);

            row = strtok_r(NULL, "\x05", &end_str);

            free(template);
        }
    }

    int size = result_html_size + strlen(result);
    char *result_page = malloc(size + 1);
    sprintf(result_page, result_start, result);

    closeHeader(req);

    esp_err_t ret = httpd_resp_send(req, result_page, HTTPD_RESP_USE_STRLEN);
    free(result_page);
    nvs_handle_t nvs;
    nvs_open(PARAM_NAMESPACE, NVS_READWRITE, &nvs);
    nvs_erase_key(nvs, "scan_result");
    nvs_commit(nvs);
    nvs_close(nvs);
    ESP_LOGI(TAG, "Requesting result page");
    return ret;
}