#include "urihandler/handler.h"

#include "router_globals.h"
#include "timer.h"

static const char *TAG = "HTTPServer";

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
    .handler = rest_handler,
};

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

httpd_uri_t clients_page_download = {
    .uri = "/clients",
    .method = HTTP_GET,
    .handler = clients_download_get_handler,
    .user_ctx = NULL};
    
httpd_uri_t ota_page_download = {
    .uri = "/ota",
    .method = HTTP_GET,
    .handler = ota_download_get_handler,
    .user_ctx = NULL};

httpd_uri_t advanced_page_download = {
    .uri = "/advanced",
    .method = HTTP_GET,
    .handler = advanced_download_get_handler,
    .user_ctx = NULL};

// URI handler for getting favicon
httpd_uri_t favicon_handler = {
    .uri = "/favicon.ico",
    .method = HTTP_GET,
    .handler = favicon_get_handler,
    .user_ctx = NULL};

httpd_uri_t jquery_handler = {
    .uri = "/jquery-8a1045d9cbf50b52a0805c111ba08e94.js",
    .method = HTTP_GET,
    .handler = jquery_get_handler,
    .user_ctx = NULL};

httpd_uri_t bootstrap_handler = {
    .uri = "/bootstrap.js",
    .method = HTTP_GET,
    .handler = bootstrap_get_handler,
    .user_ctx = NULL};

httpd_uri_t styles_handler = {
    .uri = "/styles-9ee3c4491d35b3c1d830fa9da31c7861.css",
    .method = HTTP_GET,
    .handler = styles_download_get_handler,
    .user_ctx = NULL};

httpd_handle_t start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.stack_size = 999999999;
    config.max_uri_handlers = 20;

    initializeRestartTimer();

    char *lock_pass = NULL;
    int32_t keepAlive = 0;

    get_config_param_str("lock_pass", &lock_pass);
    if (lock_pass != NULL && strlen(lock_pass) > 0)
    {
        lockUI();
        ESP_LOGI(TAG, "UI is locked with password '%s'", lock_pass);
    }
    get_config_param_int("keep_alive", &keepAlive);
    if (keepAlive == 1)
    {
        initializeKeepAliveTimer();
        ESP_LOGI(TAG, "Keep alive is enabled");
    }
    else
    {
        ESP_LOGI(TAG, "Keep alive is disabled");
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
        httpd_register_uri_handler(server, &advanced_page_download);
        httpd_register_uri_handler(server, &clients_page_download);
        httpd_register_uri_handler(server, &ota_page_download);
        httpd_register_err_handler(server, HTTPD_404_NOT_FOUND, http_404_error_handler);
        return server;
    }

    ESP_LOGI(TAG, "Error starting server!");
    return NULL;
}