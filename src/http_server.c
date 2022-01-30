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

bool isLocked = false;

char *appliedSSID = NULL;

const char *JSON_TEMPLATE = "{\"clients\": %s,\"strength\": %s,\"text\": \"%s\",\"symbol\": \"%s\"}";

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

static esp_err_t result_download_get_handler(httpd_req_t *req);
static esp_err_t index_get_handler(httpd_req_t *req);
static esp_err_t unlock_handler(httpd_req_t *req)
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
        char unlockParam[req->content_len];
        if (httpd_query_key_value(buf, "unlock", unlockParam, sizeof(unlockParam)) == ESP_OK)
        {

            preprocess_string(unlockParam);
            ESP_LOGI(TAG, "Found unlock parameter => %s (%d)", unlockParam, strlen(unlockParam));

            if (strlen(unlockParam) > 0)
            {
                char *lock;
                get_config_param_str("lock_pass", &lock);
                if (strcmp(lock, unlockParam) == 0)
                {
                    isLocked = false;
                    return index_get_handler(req);
                }
            }
        }
    }
    if (req->method == HTTP_GET) // Relock if called
    {
        isLocked = true;
        ESP_LOGI(TAG, "UI relocked");
    }
    extern const char ul_start[] asm("_binary_unlock_html_start");
    extern const char ul_end[] asm("_binary_unlock_html_end");
    const size_t ul_html_size = (ul_end - ul_start);

    setCloseHeader(req);
    return httpd_resp_send(req, ul_start, ul_html_size);
}

static esp_err_t reset_get_handler(httpd_req_t *req)
{
    httpd_req_to_sockfd(req);
    extern const char reset_start[] asm("_binary_reset_html_start");
    extern const char reset_end[] asm("_binary_reset_html_end");
    const size_t reset_html_size = (reset_end - reset_start);

    setCloseHeader(req);

    esp_err_t ret = httpd_resp_send(req, reset_start, reset_html_size);
    return ret;
}

static esp_err_t index_get_handler(httpd_req_t *req)
{
    if (isLocked)
    {
        return unlock_handler(req);
    }

    char *result_param = NULL;
    get_config_param_str("scan_result", &result_param);
    if (result_param != NULL)
    {
        free(result_param);
        ESP_LOGI(TAG, "Scan result is available. Forwarding to scan page");
        return result_download_get_handler(req);
    }

    httpd_req_to_sockfd(req);
    extern const char config_start[] asm("_binary_config_html_start");
    extern const char config_end[] asm("_binary_config_html_end");
    const size_t config_html_size = (config_end - config_start);

    char *display = NULL;

    char *lock_pass = NULL;
    get_config_param_str("lock_pass", &lock_pass);
    if (lock_pass != NULL && strlen(lock_pass) > 0)
    {
        display = "block";
    }
    else
    {
        display = "none";
    }

    size_t size = strlen(ap_ssid) + strlen(ap_passwd) + strlen(display);
    if (appliedSSID != NULL && strlen(appliedSSID) > 0)
    {
        size = size + strlen(appliedSSID);
    }
    else
    {
        size = size + strlen(ssid) + strlen(passwd);
    }
    char *clients = malloc(6);
    char *db = malloc(5);
    char *symbol = NULL;
    char *textColor = NULL;
    fillInfoData(&clients, &db, &symbol, &textColor);

    size = size + strlen(symbol) + strlen(textColor) + strlen(clients) + strlen(db);
    ESP_LOGI(TAG, "Allocating additional %d bytes for config page.", config_html_size + size);
    char *config_page = malloc(config_html_size + size);

    if (appliedSSID != NULL && strlen(appliedSSID) > 0)
    {
        sprintf(config_page, config_start, clients, ap_ssid, ap_passwd, textColor, symbol, db, appliedSSID, "", display);
    }
    else
    {
        sprintf(config_page, config_start, clients, ap_ssid, ap_passwd, textColor, symbol, db, ssid, passwd, display);
    }

    setCloseHeader(req);

    esp_err_t ret = httpd_resp_send(req, config_page, config_html_size + size - 18); // 9 *2 for parameter substitution (%s)
    free(config_page);
    free(appliedSSID);
    free(clients);
    free(db);
    return ret;
}

static esp_err_t index_post_handler(httpd_req_t *req)
{

    if (isLocked)
    {
        return unlock_handler(req);
    }
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
            ESP_LOGI(TAG, "Found SSID parameter => %s (%d)", ssidParam, strlen(ssidParam));
            if (strlen(ssidParam) > 0)
            {
                appliedSSID = malloc(strlen(ssidParam) + 1);
                strcpy(appliedSSID, ssidParam);
            }
        }
    }

    return index_get_handler(req);
}

static esp_err_t apply_get_handler(httpd_req_t *req)
{

    if (isLocked)
    {
        return unlock_handler(req);
    }
    extern const char apply_start[] asm("_binary_apply_html_start");
    extern const char apply_end[] asm("_binary_apply_html_end");
    const size_t apply_html_size = (apply_end - apply_start);
    ESP_LOGI(TAG, "Requesting apply page");
    setCloseHeader(req);
    return httpd_resp_send(req, apply_start, apply_html_size);
}
static esp_err_t apply_post_handler(httpd_req_t *req)
{
    if (isLocked)
    {
        return unlock_handler(req);
    }
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
    return apply_get_handler(req);
}

static esp_err_t api_handler(httpd_req_t *req)
{
    if (isLocked)
    {
        return httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, NULL);
    }
    httpd_resp_set_type(req, "application/json");

    char *clients = malloc(6);
    char *db = malloc(5);
    char *symbol = NULL;
    char *textColor = NULL;
    fillInfoData(&clients, &db, &symbol, &textColor);

    size_t size = strlen(JSON_TEMPLATE) + strlen(clients) + strlen(db) + strlen(textColor) + strlen(symbol);
    char *json = malloc(size);
    sprintf(json, JSON_TEMPLATE, clients, db, textColor, symbol);
    esp_err_t ret = httpd_resp_send(req, json, size - 8);
    ESP_LOGI(TAG, "JSON-Response: %s", json);
    free(json);
    free(clients);
    free(db);
    return ret;
}

static esp_err_t lock_handler(httpd_req_t *req)
{
    if (isLocked)
    {
        return unlock_handler(req);
    }
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
        char passParam[req->content_len], pass2Param[req->content_len];
        if (httpd_query_key_value(buf, "lockpass", passParam, sizeof(passParam)) == ESP_OK)
        {
            preprocess_string(passParam);
            ESP_LOGI(TAG, "Found pass parameter => %s (%d)", passParam, strlen(passParam));
            httpd_query_key_value(buf, "lockpass2", pass2Param, sizeof(pass2Param));
            preprocess_string(pass2Param);
            ESP_LOGI(TAG, "Found pass2 parameter => %s (%d)", pass2Param, strlen(pass2Param));
            if (strlen(passParam) == strlen(pass2Param) && strcmp(passParam, pass2Param) == 0)
            {
                ESP_LOGI(TAG, "Passes are equal. Password will be changed.");
                if (strlen(passParam) == 0)
                {
                    ESP_LOGI(TAG, "Pass will be removed");
                }
                nvs_handle_t nvs;
                nvs_open(PARAM_NAMESPACE, NVS_READWRITE, &nvs);
                nvs_set_str(nvs, "lock_pass", passParam);
                nvs_commit(nvs);
                nvs_close(nvs);
                esp_timer_start_once(restart_timer, 500000);
                return apply_get_handler(req);
            }
            else
            {
                ESP_LOGI(TAG, "Passes are not equal.");
            }
        }
    }
    extern const char l_start[] asm("_binary_lock_html_start");
    extern const char l_end[] asm("_binary_lock_html_end");
    const size_t l_html_size = (l_end - l_start);

    char *display = NULL;

    char *lock_pass = NULL;
    get_config_param_str("lock_pass", &lock_pass);
    if (lock_pass != NULL && strlen(lock_pass) > 0)
    {
        display = "block";
    }
    else
    {
        display = "none";
    }

    char *lock_page = malloc(l_html_size + strlen(display));

    sprintf(lock_page, l_start, display);

    setCloseHeader(req);

    esp_err_t out = httpd_resp_send(req, lock_page, strlen(lock_page) - 2);
    free(lock_page);
    return out;
}

static httpd_uri_t applyp = {
    .uri = "/apply",
    .method = HTTP_POST,
    .handler = apply_post_handler,
};

static httpd_uri_t applyg = {
    .uri = "/apply",
    .method = HTTP_GET,
    .handler = apply_get_handler,
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

static httpd_uri_t resetg = {
    .uri = "/reset",
    .method = HTTP_GET,
    .handler = reset_get_handler,
};

static httpd_uri_t unlockg = {
    .uri = "/unlock",
    .method = HTTP_GET,
    .handler = unlock_handler,
};
static httpd_uri_t unlockp = {
    .uri = "/unlock",
    .method = HTTP_POST,
    .handler = unlock_handler,
};

static httpd_uri_t lockp = {
    .uri = "/lock",
    .method = HTTP_POST,
    .handler = lock_handler,
};
static httpd_uri_t lockg = {
    .uri = "/lock",
    .method = HTTP_GET,
    .handler = lock_handler,
};
static httpd_uri_t apig = {
    .uri = "/api",
    .method = HTTP_GET,
    .handler = api_handler,
};

static esp_err_t scan_download_get_handler(httpd_req_t *req)
{

    if (isLocked)
    {
        return unlock_handler(req);
    }

    httpd_req_to_sockfd(req);

    extern const char scan_start[] asm("_binary_scan_html_start");
    extern const char scan_end[] asm("_binary_scan_html_end");
    const size_t scan_html_size = (scan_end - scan_start);

    setCloseHeader(req);

    ESP_LOGI(TAG, "Requesting scan page");

    esp_err_t ret = httpd_resp_send(req, scan_start, scan_html_size);
    fillNodes();
    return ret;
}

static esp_err_t result_download_get_handler(httpd_req_t *req)
{
    if (isLocked)
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

    setCloseHeader(req);

    esp_err_t ret = httpd_resp_send(req, result_page, strlen(result_page) - 2);
    free(result_page);
    nvs_handle_t nvs;
    nvs_open(PARAM_NAMESPACE, NVS_READWRITE, &nvs);
    nvs_erase_key(nvs, "scan_result");
    nvs_commit(nvs);
    nvs_close(nvs);
    return ret;
}
// URI handler for getting "html page" file
httpd_uri_t scan_page_download = {
    .uri = "/scan",
    .method = HTTP_GET,
    .handler = scan_download_get_handler,
    .user_ctx = NULL};

httpd_uri_t result_page_download = {
    .uri = "/result",
    .method = HTTP_GET,
    .handler = result_download_get_handler,
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

static esp_err_t jquery_get_handler(httpd_req_t *req)
{
    extern const unsigned char jquery_js_start[] asm("_binary_jquery_js_start");
    extern const unsigned char jquery_js_end[] asm("_binary_jquery_js_end");
    const size_t jquery_js_size = (jquery_js_end - jquery_js_start);
    httpd_resp_set_type(req, "text/javascript");
    ESP_LOGI(TAG, "Requesting jquery");
    return downloadStatic(req, (const char *)jquery_js_start, jquery_js_size);
}

static esp_err_t bootstrap_get_handler(httpd_req_t *req)
{
    extern const unsigned char bootstrap_js_start[] asm("_binary_bootstrap_js_start");
    extern const unsigned char bootstrap_js_end[] asm("_binary_bootstrap_js_end");
    const size_t bootstrap_js_size = (bootstrap_js_end - bootstrap_js_start);
    httpd_resp_set_type(req, "text/javascript");
    ESP_LOGI(TAG, "Requesting bootstrap");
    return downloadStatic(req, (const char *)bootstrap_js_start, bootstrap_js_size);
}

// URI handler for getting favicon
httpd_uri_t favicon_handler = {
    .uri = "/favicon.ico",
    .method = HTTP_GET,
    .handler = favicon_get_handler,
    .user_ctx = NULL};

httpd_uri_t jquery_handler = {
    .uri = "/jquery.js",
    .method = HTTP_GET,
    .handler = jquery_get_handler,
    .user_ctx = NULL};

httpd_uri_t bootstrap_handler = {
    .uri = "/bootstrap.js",
    .method = HTTP_GET,
    .handler = bootstrap_get_handler,
    .user_ctx = NULL};

static esp_err_t styles_download_get_handler(httpd_req_t *req)
{
    extern const unsigned char styles_start[] asm("_binary_styles_1_css_start");
    extern const unsigned char styles_end[] asm("_binary_styles_1_css_end");
    const size_t styles_size = (styles_end - styles_start);
    httpd_resp_set_type(req, "text/css");
    ESP_LOGI(TAG, "Requesting style.css");
    return downloadStatic(req, (const char *)styles_start, styles_size);
}

httpd_uri_t styles_handler = {
    .uri = "/styles-1.css",
    .method = HTTP_GET,
    .handler = styles_download_get_handler,
    .user_ctx = NULL};

httpd_handle_t start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.stack_size = 20000;
    config.max_uri_handlers = 20;

    esp_timer_create(&restart_timer_args, &restart_timer);

    char *lock_pass = NULL;

    get_config_param_str("lock_pass", &lock_pass);
    if (lock_pass != NULL && strlen(lock_pass) > 0)
    {
        isLocked = true;
        ESP_LOGI(TAG, "UI is locked with password '%s'", lock_pass);
    }

    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK)
    {
        // Set URI handlers
        ESP_LOGI(TAG, "Registering URI handlers");
        httpd_register_uri_handler(server, &indexp);
        httpd_register_uri_handler(server, &indexg);
        httpd_register_uri_handler(server, &applyg);
        httpd_register_uri_handler(server, &applyp);
        httpd_register_uri_handler(server, &resetg);
        httpd_register_uri_handler(server, &scan_page_download);
        httpd_register_uri_handler(server, &result_page_download);
        httpd_register_uri_handler(server, &unlockg);
        httpd_register_uri_handler(server, &unlockp);
        httpd_register_uri_handler(server, &lockg);
        httpd_register_uri_handler(server, &lockp);
        httpd_register_uri_handler(server, &favicon_handler);
        httpd_register_uri_handler(server, &jquery_handler);
        httpd_register_uri_handler(server, &bootstrap_handler);
        httpd_register_uri_handler(server, &styles_handler);
        httpd_register_uri_handler(server, &apig);
        return server;
    }

    ESP_LOGI(TAG, "Error starting server!");
    return NULL;
}