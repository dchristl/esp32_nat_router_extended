#include <esp_log.h>
#include <esp_http_server.h>
#include <sys/param.h>

void preprocess_string(char *str);
void readUrlParameterIntoBuffer(char *parameterString, char *parameter, char *buffer, size_t paramLength);
esp_err_t fill_post_buffer(httpd_req_t *req, char *buf, size_t len);