#include "helper.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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