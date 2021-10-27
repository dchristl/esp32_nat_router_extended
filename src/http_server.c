#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <sys/param.h>
#include "esp_netif.h"

#include <esp_http_server.h>

#include "router_globals.h"
#include "helper.h"
#include "cmd_nvs.h"
#include "nvs.h"

esp_timer_handle_t restart_timer;

static void restart_timer_callback(void *arg)
{
    ESP_LOGI(TAG, "Restarting now...");
    esp_restart();
}

esp_timer_create_args_t restart_timer_args = {
    .callback = &restart_timer_callback,
    /* argument specified here will be passed to timer callback function */
    .arg = (void *)0,
    .name = "restart_timer"};

static esp_err_t index_get_handler(httpd_req_t *req)
{
    httpd_req_to_sockfd(req);
    extern const char config_start[] asm("_binary_config_html_start");
    extern const char config_end[] asm("_binary_config_html_end");
    const size_t config_html_size = (config_end - config_start);
    size_t size = strlen(ap_ssid) + strlen(ap_passwd) + strlen(ssid) + strlen(passwd);
    ESP_LOGD(TAG, "Allocating additional %d bytes for config page.", size);
    char *config_page = malloc(config_html_size + size);

    sprintf(config_page, config_start, ap_ssid, ap_passwd, ssid, passwd);

    ESP_LOGI(TAG, "Requesting config page");

    setCloseHeader(req);

    esp_err_t ret = httpd_resp_send(req, config_page, strlen(config_page));
    free(config_page);

    return ret;
}

static esp_err_t index_post_handler(httpd_req_t *req)
{
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
            ESP_LOGI(TAG, "Found SSID parameter => %s", ssidParam);
            if (strlen(ssidParam) > 0)
            {
                ap_ssid = ssidParam;
                ap_passwd = "";
            }
        }
    }

    return index_get_handler(req);
}
static esp_err_t apply_post_handler(httpd_req_t *req)
{
    httpd_req_to_sockfd(req);
    extern const char apply_start[] asm("_binary_apply_html_start");
    extern const char apply_end[] asm("_binary_apply_html_end");
    const size_t apply_html_size = (apply_end - apply_start);

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
        char funcParam[req->content_len];
        if (httpd_query_key_value(buf, "func", funcParam, sizeof(funcParam)) == ESP_OK)
        {
            ESP_LOGI(TAG, "Found function parameter => %s", funcParam);
            preprocess_string(funcParam);
            if (strcmp(funcParam, "erase") == 0)
            {
                ESP_LOGW(TAG, "Erasing %s", PARAM_NAMESPACE);
                int argc = 2;
                char *argv[argc];
                argv[0] = "erase_namespace";
                argv[1] = PARAM_NAMESPACE;
                erase_ns(argc, argv);
            }
            esp_timer_start_once(restart_timer, 500000);
        }
        else
        {
            char *postCopy;
            postCopy = malloc(sizeof(char) * (strlen(buf) + 1));
            strcpy(postCopy, buf);
            nvs_handle_t nvs;
            nvs_open(PARAM_NAMESPACE, NVS_READWRITE, &nvs);
            setApByQuery(postCopy, nvs);
            setStaByQuery(postCopy, nvs);
            nvs_commit(nvs);
            nvs_close(nvs);
            free(postCopy);
            esp_timer_start_once(restart_timer, 500000);
        }
    }
    ESP_LOGI(TAG, "Requesting apply page");
    setCloseHeader(req);
    return httpd_resp_send(req, apply_start, apply_html_size);
}

static httpd_uri_t applyp = {
    .uri = "/apply",
    .method = HTTP_POST,
    .handler = apply_post_handler,
};

static httpd_uri_t indexg = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = index_get_handler,
};
static httpd_uri_t indexp = {
    .uri = "/",
    .method = HTTP_POST,
    .handler = index_post_handler,
};

static esp_err_t scan_download_get_handler(httpd_req_t *req)
{
    httpd_req_to_sockfd(req);

    extern const char scan_start[] asm("_binary_scan_html_start");
    extern const char scan_end[] asm("_binary_scan_html_end");
    const size_t scan_html_size = (scan_end - scan_start);

    const char *scan_result = fillNodes();

    int size = scan_html_size + strlen(scan_result);
    char *scan_page = malloc(size);
    sprintf(scan_page, scan_start, scan_result);

    setCloseHeader(req);

    ESP_LOGI(TAG, "Requesting scan page");

    esp_err_t ret = httpd_resp_send(req, scan_page, strlen(scan_page));
    free(scan_page);
    return ret;
}
// URI handler for getting "html page" file
httpd_uri_t scan_page_download = {
    .uri = "/scan",
    .method = HTTP_GET,
    .handler = scan_download_get_handler,
    .user_ctx = NULL};

esp_err_t http_404_error_handler(httpd_req_t *req, httpd_err_code_t err)
{
    httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Page not found");
    return ESP_FAIL;
}

// Handler to download a "favicon.ico" file kept on the server
static esp_err_t favicon_get_handler(httpd_req_t *req)
{
    extern const unsigned char favicon_ico_start[] asm("_binary_favicon_ico_start");
    extern const unsigned char favicon_ico_end[] asm("_binary_favicon_ico_end");
    const size_t favicon_ico_size = (favicon_ico_end - favicon_ico_start);
    httpd_resp_set_type(req, "image/x-icon");
    ESP_LOGI(TAG, "Requesting favicon");
    return downloadStatic(req, (const char *)favicon_ico_start, favicon_ico_size);
}

// URI handler for getting favicon
httpd_uri_t favicon_handler = {
    .uri = "/favicon.ico",
    .method = HTTP_GET,
    .handler = favicon_get_handler,
    .user_ctx = NULL};

static esp_err_t styles_download_get_handler(httpd_req_t *req)
{
    extern const unsigned char styles_start[] asm("_binary_styles_css_start");
    extern const unsigned char styles_end[] asm("_binary_styles_css_end");
    const size_t styles_size = (styles_end - styles_start);
    httpd_resp_set_type(req, "text/css");
    ESP_LOGI(TAG, "Requesting style.css");
    return downloadStatic(req, (const char *)styles_start, styles_size);
}

httpd_uri_t styles_handler = {
    .uri = "/styles.css",
    .method = HTTP_GET,
    .handler = styles_download_get_handler,
    .user_ctx = NULL};

httpd_handle_t start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.stack_size = 12000;

    esp_timer_create(&restart_timer_args, &restart_timer);

    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK)
    {
        // Set URI handlers
        ESP_LOGI(TAG, "Registering URI handlers");
        httpd_register_uri_handler(server, &indexp);
        httpd_register_uri_handler(server, &indexg);
        httpd_register_uri_handler(server, &applyp);
        httpd_register_uri_handler(server, &scan_page_download);
        httpd_register_uri_handler(server, &favicon_handler);
        httpd_register_uri_handler(server, &styles_handler);
        return server;
    }

    ESP_LOGI(TAG, "Error starting server!");
    return NULL;
}

// static void stop_webserver(httpd_handle_t server)
// {
//     // Stop the httpd server
//     httpd_stop(server);
// }