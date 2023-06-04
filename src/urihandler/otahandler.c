
#include "handler.h"
#include <esp_ota_ops.h>
#include <esp_https_ota.h>
#include <esp_log.h>

static const char *TAG = "OTA";

static const char *DEFAULT_URL = "https://raw.githubusercontent.com/dchristl/esp32_nat_router_extended/gh-pages/version";

void ota_task(void *pvParameter)
{
    esp_http_client_config_t config = {
        .url = "https://example.com/firmware.bin",
        .skip_cert_common_name_check = true};

    esp_err_t ret = esp_https_ota(&config);
    if (ret == ESP_OK)
    {
        esp_restart();
    }
    else
    {
        ESP_LOGE(TAG, "OTA update failed! Error: %d", ret);
    }

    vTaskDelete(NULL);
}

void start_ota_update()
{
    xTaskCreate(&ota_task, "ota_task", 8192, NULL, 5, NULL);
}

esp_err_t ota_download_get_handler(httpd_req_t *req)
{

    if (isLocked())
    {
        return unlock_handler(req);
    }

    httpd_req_to_sockfd(req);


    extern const char ota_start[] asm("_binary_ota_html_start");
    extern const char ota_end[] asm("_binary_ota_html_end");
    const size_t ota_html_size = (ota_end - ota_start);

    char *ota_page = malloc(ota_html_size /* + strlen(defaultIP) */);

    sprintf(ota_page, ota_start);

    closeHeader(req);

    ESP_LOGI(TAG, "Requesting OTA page");

    esp_err_t ret = httpd_resp_send(req, ota_page, HTTPD_RESP_USE_STRLEN);
    return ret;
}