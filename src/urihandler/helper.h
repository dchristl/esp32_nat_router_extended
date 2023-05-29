#include <esp_log.h>
#include <esp_http_server.h>

void preprocess_string(char *str);
void readUrlParameterIntoBuffer(char *parameterString, char *parameter, char *buffer, size_t paramLength);