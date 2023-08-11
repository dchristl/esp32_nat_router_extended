
#include "handler.h"
#include <esp_ota_ops.h>
#include <esp_https_ota.h>
#include <esp_log.h>
#include <sys/param.h>
#include "timer.h"
#include <esp_http_client.h>

static const char *TAG = "OTA";

static const char *NOT_DETERMINED = "Not determined yet";
static const char *ERROR_RETRIEVING = "Error retrieving the data. HTTP-Code: %d";
static char latest_version[50] = "";
static char changelog[400] = "";
bool finished = false;
bool otaRunning = false;

char chip_type[30];

char otalog[400] = "";
char resultLog[110] = "";
char progressLabel[20] = "";

typedef struct
{
    int http_code;
} http_handler_data_t;

static const char *DEFAULT_URL = "https://raw.githubusercontent.com/dchristl/esp32_nat_router_extended/releases-production/";
static const char *DEFAULT_URL_CANARY = "https://raw.githubusercontent.com/dchristl/esp32_nat_router_extended/releases-staging/";

void appendToLog(const char *message)
{
    char tmp[500] = "";

    sprintf(tmp, "<tr><th>%s</th></tr>", message);

    strcat(otalog, tmp);
    ESP_LOGI(TAG, "%s", message);
}

void setResultLog(const char *message, const char *cssClass)
{

    sprintf(resultLog, "<tr><th class=\"%s\">%s</th></tr>", cssClass, message);

    ESP_LOGI(TAG, "%s", message);
}

#define DOWNLOAD_TIMEOUT_MS 5000
char *file_buffer = NULL;
size_t file_size = 0;

double data_length = 0;
int64_t content_length = 0;
int threshold = 0;
int progressInt = 0;

// HTTP-Client-Event-Handler
esp_err_t ota_event_event_handler(esp_http_client_event_t *evt)
{
    char tmp[50] = "";

    switch (evt->event_id)
    {
    case HTTP_EVENT_ON_DATA:
        data_length = data_length + ((double)evt->data_len / 1000.0);
        double progress = (double)data_length / content_length * 100.0;
        progressInt = (int)progress;
        sprintf(progressLabel, "%.0f of %lld kB", data_length, content_length);
        if (progressInt >= threshold) // do not flood log
        {
            threshold = threshold + 10;
            ESP_LOGI(TAG, "%s", progressLabel);
        }
        break;
    case HTTP_EVENT_ERROR:
        return ESP_FAIL;
    case HTTP_EVENT_ON_HEADER:

        char *endptr;
        if (strcasecmp("Content-Length", evt->header_key) == 0)
        {
            content_length = strtol(evt->header_value, &endptr, 10) / 1000;
            sprintf(tmp, "Download size is %lld kB", content_length);
            appendToLog(tmp);
        }
        break;
    default:
        break;
    }
    return ESP_OK;
}

esp_err_t version_event_handler(esp_http_client_event_t *evt)
{

    http_handler_data_t *handler_data = (http_handler_data_t *)evt->user_data;

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
        int http_status_code = esp_http_client_get_status_code(evt->client);
        handler_data->http_code = http_status_code;
        ESP_LOGI(TAG, "Download finished");
        break;
    default:
        break;
    }
    return ESP_OK;
}

const char *get_default_url()
{
    int32_t canary = 0;
    get_config_param_int("canary", &canary);
    if (canary == 1)
    {
        return DEFAULT_URL_CANARY;
    }

    return DEFAULT_URL;
}

void getOtaUrl(char *url, char *label)
{
    char *customUrl = NULL;
    // Assuming the function get_config_param_str is defined elsewhere
    get_config_param_str("ota_url", &customUrl);
    if (customUrl != NULL && strlen(customUrl) > 0)
    {
        ESP_LOGI(TAG, "Custom Url found '%s'\n", customUrl);
        strcpy(label, "Custom build");
        strcpy(url, customUrl);
    }
    else
    {
        const char *usedUrl = get_default_url();
        if (strcmp(usedUrl, DEFAULT_URL_CANARY) == 0)
        {
            strcpy(label, "Canary build");
        }
        else
        {
            strcpy(label, "Default build");
        }

        strcpy(url, usedUrl);
        strcat(url, chip_type);
        strcat(url, "/");
        strcat(url, "firmware.bin");
    }
}

void ota_task(void *pvParameter)
{
    data_length = 0;
    content_length = 0;
    threshold = 0;
    char url[200];
    char label[20];

    getOtaUrl(url, label);

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
        setResultLog("OTA update succesful. The device is restarting.", "text-success");
    }
    else
    {
        setResultLog("Error occured. The device is restarting ", "text-danger");
    }
    finished = true;
    vTaskDelete(NULL);
}

void start_ota_update()
{
    xTaskCreate(&ota_task, "ota_task", 8192, NULL, 5, NULL);
}

void appendToChangelog(const char *entry)
{
    char tmp[500] = "";
    sprintf(tmp, "<li>%s</li>", entry);
    strcat(changelog, tmp);
}

void updateVersion()
{
    const char *usedUrl = get_default_url();
    char url[strlen(usedUrl) + 50];
    strcpy(url, usedUrl);
    strcat(url, "version");
    esp_http_client_config_t config = {
        .url = url,
        .event_handler = version_event_handler,
        .user_data = &(http_handler_data_t){.http_code = 200},
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_timeout_ms(client, DOWNLOAD_TIMEOUT_MS);
    esp_err_t err = esp_http_client_perform(client);
    changelog[0] = '\0';
    http_handler_data_t *handler_data = (http_handler_data_t *)config.user_data;

    if (err == ESP_OK && handler_data->http_code == 200)
    {
        ESP_LOGI(TAG, "Version and changelog download succesful. File size is: %d Bytes", file_size);
        file_buffer[file_size - 1] = '\0'; // Terminate the string on the correct length
        char *rest = file_buffer;
        char *line;
        int lineNumber = 1;
        while ((line = strtok_r(rest, "\n", &rest)) != NULL)
        {
            ESP_LOGD(TAG, "Line %d: %s\n", lineNumber, line);
            switch (lineNumber)
            {
            case 1:
                strcpy(latest_version, line);
                break;

            default:
                appendToChangelog(line);
                break;
            }
            lineNumber++;
        }

        free(file_buffer);
        file_buffer = NULL;
        file_size = 0;
    }
    else
    {
        ESP_LOGE(TAG, "Error on download: %s -> %d\n", esp_err_to_name(err), handler_data->http_code);
        snprintf(latest_version, sizeof(latest_version), ERROR_RETRIEVING, handler_data->http_code);
        appendToChangelog(latest_version);
    }
    esp_http_client_cleanup(client);
}
esp_err_t otalog_get_handler(httpd_req_t *req)
{

    if (isLocked())
    {
        return redirectToLock(req);
    }
    if (!otaRunning)
    {
        httpd_resp_set_status(req, "302 Found");
        httpd_resp_set_hdr(req, "Location", "/");
        return httpd_resp_send(req, NULL, 0);
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
    char url[200];
    char label[20];

    getOtaUrl(url, label);

    char *otalog_page = malloc(otalog_html_size + strlen(otalog) + strlen(otaLogRedirect) + strlen(resultLog) + strlen(progressLabel) + 50 + strlen(label));
    sprintf(otalog_page, otalog_start, otaLogRedirect, progressInt, progressLabel, label, otalog, resultLog);

    closeHeader(req);

    ESP_LOGI(TAG, "Requesting OTA-Log page");

    esp_err_t ret = httpd_resp_send(req, otalog_page, HTTPD_RESP_USE_STRLEN);
    return ret;
}

esp_err_t otalog_post_handler(httpd_req_t *req)
{
    if (isLocked())
    {
        return redirectToLock(req);
    }
    resultLog[0] = '\0';
    otalog[0] = '\0';
    otaRunning = true;
    start_ota_update();

    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "/otalog");
    return httpd_resp_send(req, NULL, 0);
}

esp_err_t ota_download_get_handler(httpd_req_t *req)
{
    if (isLocked())
    {
        return redirectToLock(req);
    }

    httpd_req_to_sockfd(req);
    extern const char ota_start[] asm("_binary_ota_html_start");
    extern const char ota_end[] asm("_binary_ota_html_end");
    const size_t ota_html_size = (ota_end - ota_start);

    if (strlen(latest_version) == 0)
    {
        strcpy(latest_version, NOT_DETERMINED);
        changelog[0] = '\0';
        appendToChangelog(NOT_DETERMINED);
    }

    determineChipType(chip_type);
    ESP_LOGD(TAG, "Chip Type: %s\n", chip_type);
    char customUrl[200];
    char label[20];
    getOtaUrl(customUrl, label);
    char *ota_page = malloc(ota_html_size + strlen(GLOBAL_VERSION) + strlen(customUrl) + strlen(latest_version) + strlen(chip_type) + strlen(label) + strlen(changelog));
    sprintf(ota_page, ota_start, GLOBAL_VERSION, latest_version, changelog, customUrl, label, chip_type);

    closeHeader(req);

    ESP_LOGI(TAG, "Requesting OTA page with additional size of %d", strlen(ota_page));

    esp_err_t ret = httpd_resp_send(req, ota_page, HTTPD_RESP_USE_STRLEN);
    free(ota_page);
    return ret;
}

esp_err_t ota_post_handler(httpd_req_t *req)
{
    if (isLocked())
    {
        return redirectToLock(req);
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

    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "/ota");
    return httpd_resp_send(req, NULL, 0);
}