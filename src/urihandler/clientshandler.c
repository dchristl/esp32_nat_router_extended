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

const char *CLIENT_TEMPLATE = "<tr><td>%i</td><td>%s</td><td style='text-transform: uppercase;'>%s</td></tr>";
const char *STATIC_IP_TEMPLATE = "<tr><td>%i</td><td>%s</td><td style='text-transform: uppercase;'>%s</td><form action='/clients' method='POST'><input type='hidden' name='func' value='del'><input type='hidden' name='entry' value='%s'> <button title='Remove' name='remove' class='btn btn-light'> <svg version='2.0' width='16' height='16'> <use href='#trash' /> </svg> </form> </td></tr>";

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

    char connected_result[1000];
    strcpy(connected_result, "");
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
            strcat(connected_result, template);

        }
    }
    else
    {
        strcat(connected_result, "<tr class='text-danger'><td colspan='3'>No clients connected</td></tr>");
    }

    // TODO: REVIEW THIS AGAINST PORTMAP PROCESS
    // static_mappings = get_static_mappings() (needs to be some sort of list)
    // char static_result[1000];
    // strcpy(result, "");
    // if static_mappings.num > 0
    //     for static_mapping (need index/count)
    //         char delParam[50];
    //         sprintf(delParam, "%s_%s_%s", index, ip_addr, mac_addr);
    //         
    //         char *template = malloc(strlen(STATIC_IP_TEMPLATE) + 100);
    //         // static_mapping = do_something_to_get_mapping() (should have ip and mac)
    //         //    look at using esp_netif_pair_mac_ip_t from above
    //         char mapping_ip[16];
    //         esp_ip4addr_ntoa(&(static_mapping.ip), mapping_ip, IP4ADDR_STRLEN_MAX )
    //         char mapping_mac[18];
    //         sprintf(mapping_mac, "%x:%x:%x:%x:%x:%x", static_mapping.mac[0], static_mapping.mac[1],
    //                                                   static_mapping.mac[2], static_mapping.mac[3],
    //                                                   static_mapping.mac[4], static_mapping.mac[5]);
    //         sprintf(template, STATIC_IP_TEMPLATE, i + 1, mapping_ip, mapping_map);
    //         strcat(static_result, template);
    // else
    //     strcat(static_result, "<tr class='text-danger'><td colspan='3'>No static IP assignments</td></tr>");

    httpd_req_to_sockfd(req);
    extern const char clients_start[] asm("_binary_clients_html_start");
    extern const char clients_end[] asm("_binary_clients_html_end");
    const size_t clients_html_size = (clients_end - clients_start);

    int size = clients_html_size + strlen(connected_result) + strlen("<tr class='text-danger'><td colspan='3'>No clients connected</td></tr>"); // + strlen(static_result)
    char *clients_page = malloc(size);
    sprintf(clients_page, clients_start, connected_result, "<tr class='text-danger'><td colspan='3'>No clients connected</td></tr>"); // , static_result);

    closeHeader(req);

    esp_err_t ret = httpd_resp_send(req, clients_page, HTTPD_RESP_USE_STRLEN);
    free(clients_page);
    ESP_LOGI(TAG, "Requesting clients page");

    return ret;
}

void addStaticIPEntry(char *urlContent)
{
    size_t contentLength = 64;
    char ip_addr[contentLength];
    char mac_addr[contentLength];

    readUrlParameterIntoBuffer(urlContent, "ipaddr", ip_addr, contentLength);
    if (strlen(ip_addr) > 0)
    {
        readUrlParameterIntoBuffer(urlContent, "macaddr", mac_addr, contentLength);
        if (strlen(mac_addr) > 0)
        {
            add_static_ip(ip_addr, mac_addr);
        }
    }
}

void delStaticIPEntry(char *urlContent)
{
    size_t contentLength = 64;
    char ip_addr[contentLength];
    char mac_addr[contentLength];
    
    readUrlParameterIntoBuffer(urlContent, "ipaddr", ip_addr, contentLength);
    if (strlen(ip_addr) > 0)
    {
        readUrlParameterIntoBuffer(urlContent, "macaddr", mac_addr, contentLength);
        if (strlen(mac_addr) > 0)
        {
            del_static_ip(ip_addr, mac_addr);
        }
    }
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
}