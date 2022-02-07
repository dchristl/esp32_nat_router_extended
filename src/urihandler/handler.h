#include <esp_log.h>
#include <esp_http_server.h>

/* Static */
void closeHeader(httpd_req_t *req);
esp_err_t styles_download_get_handler(httpd_req_t *req);
esp_err_t bootstrap_get_handler(httpd_req_t *req);
esp_err_t jquery_get_handler(httpd_req_t *req);
esp_err_t favicon_get_handler(httpd_req_t *req);
esp_err_t http_404_error_handler(httpd_req_t *req, httpd_err_code_t err);
esp_err_t reset_get_handler(httpd_req_t *req);

/* IndexHandler */
esp_err_t index_get_handler(httpd_req_t *req);
esp_err_t index_post_handler(httpd_req_t *req);

/* Lockhandler */
bool isLocked();
void lockUI();
esp_err_t unlock_handler(httpd_req_t *req);
esp_err_t lock_handler(httpd_req_t *req);

/* ScanHandler */
void fillInfoData(char **clients, char **db, char **symbol, char **textColor);
esp_err_t scan_download_get_handler(httpd_req_t *req);

/* ResultHandler */
esp_err_t result_download_get_handler(httpd_req_t *req);

/* ApplyHandler */
esp_err_t apply_get_handler(httpd_req_t *req);
esp_err_t apply_post_handler(httpd_req_t *req);

// /* RestHandler */
esp_err_t rest_handler(httpd_req_t *req);