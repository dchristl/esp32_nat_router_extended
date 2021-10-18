/* Simple HTTP Server Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <sys/param.h>
#include "esp_netif.h"

#include <esp_http_server.h>

#include "pages.h"
#include "router_globals.h"
#include "helper.h"
static const char *TAG = "HTTPServer";

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

    char *config_page = malloc(config_html_size + 512);
    sprintf(config_page, config_start, ap_ssid, ap_passwd, ssid, passwd,
            static_ip, subnet_mask, gateway_addr);

    const char *field = "Connection";
    const char *value = "close";

    httpd_resp_set_hdr(req, field, value);

    esp_err_t ret = httpd_resp_send(req, config_page, config_html_size);

    return ret;
}
/* An HTTP GET handler */
static esp_err_t index_get_handler2(httpd_req_t *req)
{
    char *buf;
    size_t buf_len;

    /* Get header value string length and allocate memory for length + 1,
     * extra byte for null termination */
    buf_len = httpd_req_get_hdr_value_len(req, "Host") + 1;

    if (buf_len > 1)
    {
        buf = malloc(buf_len);
        /* Copy null terminated value string into buffer */
        if (httpd_req_get_hdr_value_str(req, "Host", buf, buf_len) == ESP_OK)
        {
            ESP_LOGI(TAG, "Found header => Host: %s", buf);
        }
        free(buf);
    }

    /* Read URL query string length and allocate memory for length + 1,
     * extra byte for null termination */
    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1)
    {
        buf = malloc(buf_len);
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK)
        {
            ESP_LOGI(TAG, "Found URL query => %s", buf);
            if (strcmp(buf, "reset=Restart") == 0)
            {
                esp_timer_start_once(restart_timer, 500000);
            }
            char param1[64];
            char param2[64];
            char param3[64];
            /* Get value of expected key from query string */
            if (httpd_query_key_value(buf, "ap_ssid", param1, sizeof(param1)) == ESP_OK)
            {
                ESP_LOGI(TAG, "Found URL query parameter => ap_ssid=%s", param1);
                preprocess_string(param1);
                if (httpd_query_key_value(buf, "ap_password", param2, sizeof(param2)) == ESP_OK)
                {
                    ESP_LOGI(TAG, "Found URL query parameter => ap_password=%s", param2);
                    preprocess_string(param2);
                    int argc = 3;
                    char *argv[3];
                    argv[0] = "set_ap";
                    argv[1] = param1;
                    argv[2] = param2;
                    set_ap(argc, argv);
                    esp_timer_start_once(restart_timer, 500000);
                }
            }
            if (httpd_query_key_value(buf, "ssid", param1, sizeof(param1)) == ESP_OK)
            {
                ESP_LOGI(TAG, "Found URL query parameter => ssid=%s", param1);
                preprocess_string(param1);
                if (httpd_query_key_value(buf, "password", param2, sizeof(param2)) == ESP_OK)
                {
                    ESP_LOGI(TAG, "Found URL query parameter => password=%s", param2);
                    preprocess_string(param2);
                    int argc = 3;
                    char *argv[3];
                    argv[0] = "set_sta";
                    argv[1] = param1;
                    argv[2] = param2;
                    set_sta(argc, argv);
                    esp_timer_start_once(restart_timer, 500000);
                }
            }
            if (httpd_query_key_value(buf, "staticip", param1, sizeof(param1)) == ESP_OK)
            {
                ESP_LOGI(TAG, "Found URL query parameter => staticip=%s", param1);
                preprocess_string(param1);
                if (httpd_query_key_value(buf, "subnetmask", param2, sizeof(param2)) == ESP_OK)
                {
                    ESP_LOGI(TAG, "Found URL query parameter => subnetmask=%s", param2);
                    preprocess_string(param2);
                    if (httpd_query_key_value(buf, "gateway", param3, sizeof(param3)) == ESP_OK)
                    {
                        ESP_LOGI(TAG, "Found URL query parameter => gateway=%s", param3);
                        preprocess_string(param3);
                        int argc = 4;
                        char *argv[4];
                        argv[0] = "set_sta_static";
                        argv[1] = param1;
                        argv[2] = param2;
                        argv[3] = param3;
                        set_sta_static(argc, argv);
                        esp_timer_start_once(restart_timer, 500000);
                    }
                }
            }
        }
        free(buf);
    }

    /* Send response with custom headers and body set as the
     * string passed in user context*/
    const char *resp_str = (const char *)req->user_ctx;
    httpd_resp_send(req, resp_str, strlen(resp_str));

    return ESP_OK;
}

static httpd_uri_t indexp = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = index_get_handler,
};

static esp_err_t scan_download_get_handler(httpd_req_t *req)
{
    httpd_req_to_sockfd(req);

    extern const char scan_start[] asm("_binary_scan_html_start");
    extern const char scan_end[] asm("_binary_scan_html_end");
    const size_t scan_html_size = (scan_end - scan_start);

    char hello[] = "Hello World";

    char *scan_page = malloc(scan_html_size + sizeof(hello));
    sprintf(scan_page, scan_start, hello);

    const char *field = "Connection";
    const char *value = "close";

    httpd_resp_set_hdr(req, field, value);

    esp_err_t ret = httpd_resp_send(req, scan_page, scan_html_size);

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

    esp_timer_create(&restart_timer_args, &restart_timer);

    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK)
    {
        // Set URI handlers
        ESP_LOGI(TAG, "Registering URI handlers");
        httpd_register_uri_handler(server, &indexp);
        httpd_register_uri_handler(server, &scan_page_download);
        httpd_register_uri_handler(server, &favicon_handler);
        httpd_register_uri_handler(server, &styles_handler);
        return server;
    }

    ESP_LOGI(TAG, "Error starting server!");
    return NULL;
}

static void stop_webserver(httpd_handle_t server)
{
    // Stop the httpd server
    httpd_stop(server);
}