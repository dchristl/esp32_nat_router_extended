#include <esp_log.h>
#include <esp_http_server.h>

static const char *TAG_HANDLER = "UriHandler";

esp_err_t styles_download_get_handler(httpd_req_t *req);
esp_err_t bootstrap_get_handler(httpd_req_t *req);
esp_err_t jquery_get_handler(httpd_req_t *req);
esp_err_t favicon_get_handler(httpd_req_t *req);