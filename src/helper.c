#include "helper.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "router_globals.h"
#include "nvs.h"

const char *ROW_TEMPLATE = "<tr><td>%s</td><td>%s</td><td> <a href='/config.html?asas=%s' class='btn btn-success'>Use</a></td></tr>";

esp_err_t downloadStatic(httpd_req_t *req, const char *fileStart, const size_t fileSize)
{
    httpd_resp_set_hdr(req, "Cache-Control", "max-age=31536000");

    setCloseHeader(req);

    return httpd_resp_send(req, fileStart, fileSize);
}

void setCloseHeader(httpd_req_t *req)
{
    const char *field = "Connection";
    const char *value = "close";
    httpd_resp_set_hdr(req, field, value);
}

void fillParamArray(char *buf, char *argv[], char *ssidKey, char *passKey)
{
    char ssidParam[32];
    char passParam[64];

    if (httpd_query_key_value(buf, ssidKey, ssidParam, sizeof(ssidParam)) == ESP_OK)
    {
        ESP_LOGI(TAG, "Found URL query parameter => %s=%s", ssidKey, ssidParam);
        preprocess_string(ssidParam);
        httpd_query_key_value(buf, passKey, passParam, sizeof(passParam));
        ESP_LOGI(TAG, "Found URL query parameter => %s=%s", passKey, passParam);
        preprocess_string(passParam);
        strcpy(argv[1], ssidParam);
        strcpy(argv[2], passParam);
    }
}

void setApByQuery(char *buf)
{
    int argc = 3;
    char *argv[argc];
    argv[0] = "set_ap";
    fillParamArray(buf, argv, "ap_ssid", "ap_password");
    nvs_handle_t nvs;
    nvs_open(PARAM_NAMESPACE, NVS_READWRITE, &nvs);

    nvs_set_str(nvs, "ap_ssid", argv[1]);
    nvs_set_str(nvs, "ap_passwd", argv[2]);
    nvs_commit(nvs);
    nvs_close(nvs);
}

void setStaByQuery(char *buf)
{
    int argc = 3;
    char *argv[argc];
    argv[0] = "set_sta";
    fillParamArray(buf, argv, "ssid", "password");

    nvs_handle_t nvs;
    nvs_open(PARAM_NAMESPACE, NVS_READWRITE, &nvs);
    nvs_set_str(nvs, "ssid", argv[1]);
    nvs_set_str(nvs, "passwd", argv[2]);
    nvs_commit(nvs);
    nvs_close(nvs);
}

typedef struct node
{
    char *ssid;
    int db;
    int sec;
    struct node *next;
} node_t;

static void print_list(node_t *head);
static void push(node_t *head, int db, char *ssid, int sec);

int main(int argc, char *argv[])
{

    node_t *head = NULL;
    head = (node_t *)malloc(sizeof(node_t));
    if (head == NULL)
    {
        return 1;
    }

    head->db = 1;
    head->ssid = "Hallo Welt";
    head->sec = 3;
    head->next = NULL;

    head->db = 1;
    head->next = (node_t *)malloc(sizeof(node_t));
    head->next->db = 2;
    head->next->ssid = "Foo Bar";
    head->next->sec = 55;
    head->next->next = NULL;

    push(head, -99, "hans wurst", 22);

    print_list(head);
}

void push(node_t *head, int db, char *ssid, int sec)
{
    node_t *current = head;
    while (current->next != NULL)
    {
        current = current->next;
    }

    /* now we can add a new variable */
    current->next = (node_t *)malloc(sizeof(node_t));
    current->next->db = db;
    current->next->ssid = ssid;
    current->next->sec = sec;
    current->next->next = NULL;
}

void print_list(node_t *head)
{
    node_t *current = head;

    int size = strlen(ROW_TEMPLATE) + 1000;
    printf("size --> '%d'\n", size);

    char *config_page = malloc(size);
    sprintf(config_page, ROW_TEMPLATE, "Hallo", "asas", "blubbb");

    printf("out --> '%s'\n", config_page);

    while (current != NULL)
    {
        printf("SSID: %d --> %s\n", current->db, current->ssid);
        current = current->next;
    }
}