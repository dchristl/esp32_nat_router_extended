
#include "handler.h"
#include <esp_ota_ops.h>
#include <esp_https_ota.h>
#include <esp_log.h>
#include <sys/param.h>
#include "cmd_system.h"
#include "timer.h"
#include <esp_http_client.h>

static const char *TAG = "OTA";
static const char *VERSION = "DEV";
static const char *LATEST_VERSION = "Not determined yet";
static char latest_version_buffer[sizeof(LATEST_VERSION)];
static char *latest_version = latest_version_buffer;
bool finished = false;

char chip_type[30];

char otalog[4000] = "";

static const char *DEFAULT_URL = "https://raw.githubusercontent.com/dchristl/esp32_nat_router_extended/releases-production/";
// static const char *DEFAULT_URL = "https://raw.githubusercontent.com/dchristl/esp32_nat_router_extended/releases-staging/";

void appendToLog(const char *message, const char *cssClass)
{
    char tmp[500] = "";

    sprintf(tmp, "<tr class=\"%s\"><th>%s</th></tr>", cssClass, message);

    strcat(otalog, tmp);
    ESP_LOGI(TAG, "%s", message);
}

#define DOWNLOAD_TIMEOUT_MS 5000
char *file_buffer = NULL;
size_t file_size = 0;

double data_length = 0;
int64_t content_length = 0;
int threshold = 0;

// HTTP-Client-Event-Handler
esp_err_t ota_event_event_handler(esp_http_client_event_t *evt)
{
    char tmp[50] = "";

    switch (evt->event_id)
    {
    case HTTP_EVENT_ON_DATA:
        data_length = data_length + ((double)evt->data_len / 1000.0);
        double progress = (double)data_length / content_length * 100.0;
        int progressInt = (int)progress;
        if (progressInt >= threshold)
        {
            threshold = threshold + 10;
            sprintf(tmp, "%d%% downloaded (%.0f of %lld kB) <br/>", progressInt, data_length, content_length);
            appendToLog(tmp, "");
        }
        break;
    case HTTP_EVENT_ERROR:
        appendToLog("Error occured <br/>", "table-danger");
        return ESP_FAIL;
    case HTTP_EVENT_ON_HEADER:

        char *endptr;
        if (strcasecmp("Content-Length", evt->header_key) == 0)
        {
            content_length = strtol(evt->header_value, &endptr, 10) / 1000;
            sprintf(tmp, "Download size is %lld kB<br/>", content_length);
            appendToLog(tmp, "");
        }
        break;
    default:
        break;
    }
    return ESP_OK;
}

esp_err_t version_event_handler(esp_http_client_event_t *evt)
{

    switch (evt->event_id)
    {
    case HTTP_EVENT_ON_DATA:
        if (!esp_http_client_is_chunked_response(evt->client))
        {
            size_t new_size = file_size + evt->data_len;
            file_buffer = realloc(file_buffer, new_size);
            memcpy(file_buffer + file_size, evt->data, evt->data_len);
            file_size = new_size;
        }
        break;
    case HTTP_EVENT_ON_FINISH:
        ESP_LOGI(TAG, "Download finished");
        break;
    default:
        break;
    }
    return ESP_OK;
}

void ota_task(void *pvParameter)
{

    data_length = 0;
    content_length = 0;
    threshold = 0;
    char url[strlen(DEFAULT_URL) + 50];
    strcpy(url, DEFAULT_URL);
    strcat(url, chip_type);
    strcat(url, "/");
    strcat(url, "firmware.bin");
    ESP_LOGI(TAG, "OTA update started with Url: '%s'", url);

    char tmp[200] = "OTA update started with Url: '";
    strcat(tmp, url);
    strcat(tmp, "'");
    appendToLog(tmp, "");
    esp_http_client_config_t config = {
        .url = url,
        .event_handler = ota_event_event_handler,
        .skip_cert_common_name_check = true,
        .timeout_ms = DOWNLOAD_TIMEOUT_MS};

    esp_https_ota_config_t ota_config = {
        .http_config = &config,
    };
    finished = false;
    esp_err_t ret = esp_https_ota(&ota_config);
    if (ret == ESP_OK)
    {
        appendToLog("OTA update succesful. Device will reboot.", "table-success");
    }
    else
    {
        appendToLog("OTA update failed! Device will reboot.", "table-danger");
    }
    finished = true;
    vTaskDelete(NULL);
}

void start_ota_update()
{
    xTaskCreate(&ota_task, "ota_task", 8192, NULL, 5, NULL);
}

void updateVersion()
{
    char url[strlen(DEFAULT_URL) + 50];
    strcpy(url, DEFAULT_URL);
    strcat(url, "version");
    esp_http_client_config_t config = {
        .url = url,
        .event_handler = version_event_handler,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_timeout_ms(client, DOWNLOAD_TIMEOUT_MS);
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK)
    {
        strncpy(latest_version, file_buffer, file_size);
        latest_version[file_size] = '\0';
        ESP_LOGI(TAG, "Version download succesful. Latest version is '%s'. File size is: %d Bytes", file_buffer, file_size);
        free(file_buffer);
        file_buffer = NULL;
        file_size = 0;
    }
    else
    {
        latest_version = "Error retrieving the latest version";
        ESP_LOGD(TAG, "Error on download: %s\n", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
}
esp_err_t otalog_get_handler(httpd_req_t *req)
{
    if (isLocked())
    {
        return unlock_handler(req);
    }

    httpd_req_to_sockfd(req);

    extern const char otalog_start[] asm("_binary_otalog_html_start");
    extern const char otalog_end[] asm("_binary_otalog_html_end");
    const size_t otalog_html_size = (otalog_end - otalog_start);
    char *otaLogRedirect = "1; url=/otalog";
    if (finished)
    {
        otaLogRedirect = "3; url=/apply";
        restartByTimerinS(3);
    }

    char *otalog_page = malloc(otalog_html_size + strlen(otalog) + strlen(otaLogRedirect));
    sprintf(otalog_page, otalog_start, otaLogRedirect, otalog);

    closeHeader(req);

    ESP_LOGI(TAG, "Requesting OTA-Log page");

    esp_err_t ret = httpd_resp_send(req, otalog_page, HTTPD_RESP_USE_STRLEN);
    return ret;
}

esp_err_t otalog_post_handler(httpd_req_t *req)
{
    if (isLocked())
    {
        return unlock_handler(req);
    }
    start_ota_update();

    return otalog_get_handler(req);
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
    char *customUrl = "";

    char *versionCheckVisibilityTable = "table-row";
    char *versionCheckVisibility = "block";
    get_config_param_str("ota_url", &customUrl);
    if (customUrl != NULL && strlen(customUrl) > 0)
    {
        ESP_LOGI(TAG, "Custom Url found '%s'", customUrl);
        versionCheckVisibility = "none";
        versionCheckVisibilityTable = "none";
    }
    else
    {
        strcpy(customUrl, DEFAULT_URL);
        strcat(customUrl, chip_type);
        strcat(customUrl, "/");
        strcat(customUrl, "firmware.bin");
    }
    if (strlen(latest_version) == 0)
    {
        strcpy(latest_version, LATEST_VERSION); // Initialisieren
    }

    determineChipType(chip_type);

    ESP_LOGI(TAG, "Chip Type: %s\n", chip_type);

    char *ota_page = malloc(ota_html_size + strlen(VERSION) + strlen(customUrl) + strlen(latest_version) + strlen(versionCheckVisibility) + strlen(versionCheckVisibilityTable) + strlen(chip_type));
    sprintf(ota_page, ota_start, VERSION, versionCheckVisibilityTable, latest_version, customUrl, chip_type, versionCheckVisibility);

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
    buf[req->content_len] = '\0';
    ESP_LOGI(TAG, "Getting with post: %s", buf);

    updateVersion();

    return ota_download_get_handler(req);
}