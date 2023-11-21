#include "handler.h"
#include "timer.h"

#include <sys/param.h>
#include <timer.h>

#include "nvs.h"
#include "cmd_nvs.h"
#include "router_globals.h"
#include "esp_wifi.h"
#include "esp_wifi_ap_get_sta_list.h"

static const char *TAG = "ClientsHandler";

const char *CLIENT_TEMPLATE = "<tr><td>%s</td><td>%s</td><td style='text-transform: uppercase;'>%s</td></tr>";
const char *STATIC_IP_TEMPLATE = "<tr><td>%s</td><td>%s</td><td style='text-transform: uppercase;'>%s</td><form action='/clients' method='POST'><input type='hidden' name='func' value='del'><input type='hidden' name='entry' value='%s'> <button title='Remove' name='remove' class='btn btn-light'> <svg version='2.0' width='16' height='16'> <use href='#trash' /> </svg> </form> </td></tr>";


esp_err_t clients_download_get_handler(httpd_req_t *req)
{
    if (isLocked())
    {
        return redirectToLock(req);
    }

    wifi_sta_list_t wifi_sta_list;
    wifi_sta_mac_ip_list_t adapter_sta_list;
    memset(&wifi_sta_list, 0, sizeof(wifi_sta_list));
    memset(&adapter_sta_list, 0, sizeof(adapter_sta_list));
    esp_wifi_ap_get_sta_list(&wifi_sta_list);

    esp_wifi_ap_get_sta_list_with_ip(&wifi_sta_list, &adapter_sta_list);

    char result[1000];
    strcpy(result, "");
    if (wifi_sta_list.num > 0)
    {
        for (int i = 0; i < adapter_sta_list.num; i++)
        {

            char *template = malloc(strlen(CLIENT_TEMPLATE) + 100);
            esp_netif_pair_mac_ip_t station = adapter_sta_list.sta[i];

            char str_ip[16];
            esp_ip4addr_ntoa(&(station.ip), str_ip, IP4ADDR_STRLEN_MAX);

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

    // TODO: ADD LOGIC TO LOAD STATIC IP ASSIGNMENTS
    // TODO: LOGIC NEEDS TO POPULATE THE DELETE BUTTON?

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

void addStaticIPEntry(char *urlContent)
{

    // TODO: WILL NEED TO MODIFY AS NEEDED FOR STATIC IP LOGIC
    int i = 0;

    // size_t contentLength = 64;
    // char param[contentLength];
    // readUrlParameterIntoBuffer(urlContent, "protocol", param, contentLength);
    // uint8_t tcp_udp;
    // if (strcmp(param, "tcp") == 0)
    // {
    //     tcp_udp = PROTO_TCP;
    // }
    // else
    // {
    //     tcp_udp = PROTO_UDP;
    // }
    // readUrlParameterIntoBuffer(urlContent, "eport", param, contentLength);
    // char *endptr;
    // uint16_t ext_port = (uint16_t)strtoul(param, &endptr, 10);
    // if (ext_port < 1 || *endptr != '\0')
    // {
    //     ESP_LOGW(TAG, "External port out of range");
    //     return;
    // }

    // readUrlParameterIntoBuffer(urlContent, "ip", param, contentLength);
    // char *defaultIP = getDefaultIPByNetmask();
    // char resultIP[strlen(defaultIP) + strlen(param)];
    // strncpy(resultIP, defaultIP, strlen(defaultIP) - 1);
    // resultIP[strlen(defaultIP) - 1] = '\0';
    // strcat(resultIP, param);
    // free(defaultIP);
    // uint32_t int_ip = ipaddr_addr(resultIP);
    // if (int_ip == IPADDR_NONE)
    // {
    //     ESP_LOGW(TAG, "Invalid IP");
    //     return;
    // }
    // readUrlParameterIntoBuffer(urlContent, "iport", param, contentLength);
    // uint16_t int_port = (uint16_t)strtoul(param, &endptr, 10);

    // if (int_port < 1 || *endptr != '\0')
    // {
    //     ESP_LOGW(TAG, "Internal port out of range");
    //     return;
    // }

    // add_portmap(tcp_udp, ext_port, int_ip, int_port);
}

void delStaticIPEntry(char *urlContent)
{

    // TODO: WILL NEED TO MODIFY AS NEEDED FOR STATIC IP LOGIC
    int i = 0;

    // size_t contentLength = 64;
    // char param[contentLength];
    // readUrlParameterIntoBuffer(urlContent, "entry", param, contentLength);

    // const char delimiter[] = "_";

    // char *token = strtok(param, delimiter);
    // uint8_t tcp_udp;
    // if (strcmp(token, "TCP") == 0)
    // {
    //     tcp_udp = PROTO_TCP;
    // }
    // else
    // {
    //     tcp_udp = PROTO_UDP;
    // }

    // token = strtok(NULL, delimiter);
    // char *endptr;
    // uint16_t ext_port = (uint16_t)strtoul(token, &endptr, 10);
    // if (ext_port < 1 || *endptr != '\0')
    // {
    //     ESP_LOGW(TAG, "External port out of range");
    //     return;
    // }
    // token = strtok(NULL, delimiter);
    // uint32_t int_ip = ipaddr_addr(token);
    // if (int_ip == IPADDR_NONE)
    // {
    //     ESP_LOGW(TAG, "Invalid IP");
    //     return;
    // }

    // token = strtok(NULL, delimiter);
    // uint16_t int_port = (uint16_t)strtoul(token, &endptr, 10);

    // if (int_port < 1 || *endptr != '\0')
    // {
    //     ESP_LOGW(TAG, "Internal port out of range");
    //     return;
    // }

    // del_portmap(tcp_udp, ext_port, int_ip, int_port);
}


esp_err_t clients_post_handler(httpd_req_t *req)
{

    if (isLocked())
    {
        return redirectToLock(req);
    }
    httpd_req_to_sockfd(req);

    size_t content_len = req->content_len;
    char buf[content_len];

    if (fill_post_buffer(req, buf, content_len) == ESP_OK)
    {
        char funcParam[4];

        ESP_LOGI(TAG, "getting content %s", buf);

        readUrlParameterIntoBuffer(buf, "func", funcParam, 4);

        if (strcmp(funcParam, "add") == 0)
        {
            addStaticIPEntry(buf);
        }
        if (strcmp(funcParam, "del") == 0)
        {
            delStaticIPEntry(buf);
        }
    }

    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "/portmap");
    return httpd_resp_send(req, NULL, 0);

    return NULL;
}
