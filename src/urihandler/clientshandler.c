#include "handler.h"
#include "timer.h"

#include <sys/param.h>
#include <timer.h>

#include "nvs.h"
#include "cmd_nvs.h"
#include "router_globals.h"
#include "esp_wifi.h"

static const char *TAG = "ClientsHandler";

const char *CLIENT_TEMPLATE = "<tr class='text-info'><td>%i</td><td>%s</td><td style='text-transform: uppercase;'>%s</td></tr>";

esp_err_t clients_download_get_handler(httpd_req_t *req)
{
    if (isLocked())
    {
        return unlock_handler(req);
    }

    wifi_sta_list_t wifi_sta_list;
    tcpip_adapter_sta_list_t adapter_sta_list;
    memset(&wifi_sta_list, 0, sizeof(wifi_sta_list));
    memset(&adapter_sta_list, 0, sizeof(adapter_sta_list));
    esp_wifi_ap_get_sta_list(&wifi_sta_list);

    tcpip_adapter_get_sta_list(&wifi_sta_list, &adapter_sta_list);

    char result[1000];
    strcpy(result, "");
    if (wifi_sta_list.num > 0)
    {
        for (int i = 0; i < adapter_sta_list.num; i++)
        {

            char *template = malloc(strlen(CLIENT_TEMPLATE) + 100);
            tcpip_adapter_sta_info_t station = adapter_sta_list.sta[i];

            char str_ip[16];
            esp_ip4addr_ntoa(&(station.ip), str_ip, IP4ADDR_STRLEN_MAX);
            ESP_LOGI(TAG, "%s", str_ip);

            char currentMAC[18];
            sprintf(currentMAC, "%x:%x:%x:%x:%x:%x", station.mac[0], station.mac[1], station.mac[2], station.mac[3], station.mac[4], station.mac[5]);

            sprintf(template, CLIENT_TEMPLATE, i + 1, str_ip, currentMAC);
            strcat(result, template);
        }
    }
    else
    {
        strcat(result, "<tr class='text-danger'><td colspan='3'>No clients connected</td></tr>");
    }

    httpd_req_to_sockfd(req);
    extern const char clients_start[] asm("_binary_clients_html_start");
    extern const char clients_end[] asm("_binary_clients_html_end");
    const size_t clients_html_size = (clients_end - clients_start);

    int size = clients_html_size + strlen(result);
    char *clients_page = malloc(size - 2);
    sprintf(clients_page, clients_start, result);

    closeHeader(req);

    esp_err_t ret = httpd_resp_send(req, clients_page, HTTPD_RESP_USE_STRLEN);
    free(clients_page);
    ESP_LOGI(TAG, "Requesting clients page");
    return ret;
}
