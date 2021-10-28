#include <stdio.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_http_server.h>
#include "esp_netif.h"
#include <esp_log.h>
#include "nvs.h"

static const char *TAG = "HTTPServer";


typedef struct node
{
    uint8_t ssid[33];
    int8_t db;
    int sec;
    struct node *next;
} node_t;


esp_err_t downloadStatic(httpd_req_t *req, const char *fileStart, const size_t fileSize);
void setCloseHeader(httpd_req_t *req);
void setApByQuery(char *buf, nvs_handle_t nvs);
void setStaByQuery(char *buf, nvs_handle_t nvs);
char * fillNodes();
void push(node_t *head, int db, uint8_t ssid[33], int sec);