#include <stdio.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_http_server.h>
#include "esp_netif.h"

esp_err_t downloadStatic(httpd_req_t *req, const char *fileStart, const size_t fileSize);
void setCloseHeader(httpd_req_t *req);