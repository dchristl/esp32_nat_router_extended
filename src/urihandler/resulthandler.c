#include "handler.h"

#include <esp_wifi.h>

#include "nvs.h"
#include "cmd_nvs.h"
#include "router_globals.h"

static const char *TAG = "ResultHandler";

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
    get_config_param_str("scan_result", &result_param);
    if (result_param == NULL)
    {
        result_param = "<tr class='text-danger'><td colspan='3'>No networks found</td></tr>";
    }

    int size = result_html_size + strlen(result_param);
    char *result_page = malloc(size + 1);
    sprintf(result_page, result_start, result_param);

    closeHeader(req);

    esp_err_t ret = httpd_resp_send(req, result_page, strlen(result_page) - 2);
    free(result_page);
    nvs_handle_t nvs;
    nvs_open(PARAM_NAMESPACE, NVS_READWRITE, &nvs);
    nvs_erase_key(nvs, "scan_result");
    nvs_commit(nvs);
    nvs_close(nvs);
    ESP_LOGI(TAG, "Requesting result page");
    return ret;
}