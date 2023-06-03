
#include "handler.h"
#include <esp_ota_ops.h>
#include <esp_https_ota.h>
#include <esp_log.h>

static const char *TAG = "OTA";

void ota_task(void *pvParameter)
{
    esp_http_client_config_t config = {
        .url = "https://example.com/firmware.bin",
        .skip_cert_common_name_check = true,
        .skip_cert_date_check = true,
        .skip_cert_issuer_check = true,
    };

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