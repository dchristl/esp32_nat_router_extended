#include <esp_log.h>
#include <esp_http_server.h>
#include <sys/param.h>
#include <lwip/inet.h>

void preprocess_string(char *str);
void readUrlParameterIntoBuffer(char *parameterString, char *parameter, char *buffer, size_t paramLength);
esp_err_t fill_post_buffer(httpd_req_t *req, char *buf, size_t len);
bool is_valid_subnet_mask(char *subnet_mask);
// Returns a newly malloc'd, HTML-attribute-safe copy of src (escapes
// & < > " '). Caller must free(). Returns NULL only on allocation failure.
char *html_escape(const char *src);