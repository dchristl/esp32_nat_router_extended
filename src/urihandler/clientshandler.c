#include "handler.h"
#include "timer.h"

#include <sys/param.h>
#include <timer.h>

#include "nvs.h"
#include "cmd_nvs.h"
#include "router_globals.h"
#include "esp_wifi.h"

static const char *TAG = "ClientsHandler";

esp_err_t clients_download_get_handler(httpd_req_t *req)
{
    if (isLocked())
    {
        return unlock_handler(req);
    }

    httpd_req_to_sockfd(req);
    extern const char clients_start[] asm("_binary_clients_html_start");
    extern const char clients_end[] asm("_binary_clients_html_end");
    const size_t clients_html_size = (clients_end - clients_start);

    char *result_param = NULL;
    char result[1000];
    strcpy(result, "");

    if (result_param == NULL)
    {
        strcat(result, "<tr class='text-danger'><td colspan='3'>No clients connected</td></tr>");
    }

    int size = clients_html_size + strlen(result);
    char *clients_page = malloc(size + 1);
    sprintf(clients_page, clients_start, result);

    closeHeader(req);

    esp_err_t ret = httpd_resp_send(req, clients_page, HTTPD_RESP_USE_STRLEN);
    free(clients_page);
    ESP_LOGI(TAG, "Requesting clients page");
    return ret;
}
