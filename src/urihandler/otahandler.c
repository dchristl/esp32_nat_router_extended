
#include "handler.h"
#include <esp_ota_ops.h>
#include <esp_https_ota.h>
#include <esp_log.h>

static const char *TAG = "OTA";
static const char *VERSION = "DEV";
static const char *LATEST_VERSION = "Not determined yet";
static char latest_version_buffer[sizeof(LATEST_VERSION)];
static char *latest_version = latest_version_buffer;

static const char *DEFAULT_URL = "https://raw.githubusercontent.com/dchristl/esp32_nat_router_extended/gh-pages/";

void ota_task(void *pvParameter)
{

    char url[strlen(DEFAULT_URL) + 50];
    strcpy(url, DEFAULT_URL);
    strcat(url, "esp32_firmware.bin");
    ESP_LOGI(TAG, "OTA update started with Url: '%s'", url);
    esp_http_client_config_t config = {
        .url = url,
        .skip_cert_common_name_check = true};

    esp_https_ota_config_t ota_config = {
        .http_config = &config,
    };

    esp_err_t ret = esp_https_ota(&ota_config);
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
    char *customUrl = NULL;
    get_config_param_str("ota_url", &customUrl);
    if (customUrl != NULL && strlen(customUrl) > 0)
    {
        ESP_LOGI(TAG, "Custom Url found '%s'", customUrl);
    }
    else
    {
        customUrl = "Project Url";
    }
    if (strlen(latest_version) == 0)
    {
        strcpy(latest_version, LATEST_VERSION); // Initialisieren
    }
    // if (strcmp(latest_version, LATEST_VERSION) == 0)
    // {
    //     latest_version = "hghg";
    // }

    char *ota_page = malloc(ota_html_size + strlen(VERSION) + strlen(customUrl) + strlen(latest_version));
    sprintf(ota_page, ota_start, VERSION, latest_version, customUrl);

    closeHeader(req);

    ESP_LOGI(TAG, "Requesting OTA page");

    esp_err_t ret = httpd_resp_send(req, ota_page, HTTPD_RESP_USE_STRLEN);
    return ret;
}

esp_err_t ota_post_handler(httpd_req_t *req)
{
    if (isLocked())
    {
        return unlock_handler(req);
    }
    // FIXME Start only when function pressed
    start_ota_update();

    char funcParam[12];

    int bufferLength = req->content_len;
    char content[bufferLength];

    ESP_LOGI(TAG, "Getting content %s", content);

    readUrlParameterIntoBuffer(content, "func", funcParam, 9);
        if (strcmp(funcParam, "config") == 0)
    {
        applyApStaConfig(content);
    }
    if (strcmp(funcParam, "erase") == 0)
    {
        eraseNvs();
    }
    return ota_download_get_handler(req);
}