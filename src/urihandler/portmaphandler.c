#include "handler.h"
#include "timer.h"

#include <sys/param.h>
#include <timer.h>

#include "nvs.h"
#include "cmd_nvs.h"
#include "router_globals.h"
#include "esp_wifi.h"
#include "esp_wifi_ap_get_sta_list.h"

static const char *TAG = "PortMapHandler";
const char *PORTMAP_ROW_TEMPLATE = "<tr> <td>%s</td> <td>8080</td> <td>192.168.4.2</td> <td>80</td> <td> <form action='/portmap' method='POST'><input type='hidden' name='entry' value='%s'> <button title='Remove' name='remove' class='btn btn-primary'> <svg version='2.0' width='16' height='16'> <use href='#trash' /> </svg> </form> </td></tr>";

esp_err_t portmap_get_handler(httpd_req_t *req)
{
    if (isLocked())
    {
        return redirectToLock(req);
    }

    httpd_req_to_sockfd(req);
    extern const char portmap_start[] asm("_binary_portmap_html_start");
    extern const char portmap_end[] asm("_binary_portmap_html_end");
    const size_t portmap_html_size = (portmap_end - portmap_start);
    int allocatedSize = (strlen(PORTMAP_ROW_TEMPLATE) + 100) * PORTMAP_MAX; // *entries

    char result[allocatedSize];
    strcpy(result, "");
    strcat(result, PORTMAP_ROW_TEMPLATE);

    for (int i = 0; i < PORTMAP_MAX; i++)
    {
        if (portmap_tab[i].valid)
        {

            char *template = malloc(strlen(PORTMAP_ROW_TEMPLATE) + 100);
            char *protocol = NULL;
            if (portmap_tab[i].proto == PROTO_TCP)
            {
                protocol = "TCP"
            }
            else
            {
                protocol = "UDP"
            }

            sprintf(template, PORTMAP_ROW_TEMPLATE, css, ssid, css, rssi, ssid);
            strcat(result, template);
            free(template);

            ESP_LOGI(TAG, "%s", portmap_tab[i].proto == PROTO_TCP ? "TCP " : "UDP ");
            esp_ip4_addr_t addr;
            addr.addr = my_ip;
            ESP_LOGI(TAG, IPSTR ":%d -> ", IP2STR(&addr), portmap_tab[i].mport);
            addr.addr = portmap_tab[i].daddr;
            ESP_LOGI(TAG, IPSTR ":%d\n", IP2STR(&addr), portmap_tab[i].dport);
        }
    }

    char *defaultIP = getDefaultIPByNetmask();
    char ip_prefix[strlen(defaultIP) - 1];
    strncpy(ip_prefix, defaultIP, strlen(defaultIP) - 1); // Without the last part
    ip_prefix[strlen(defaultIP) - 1] = '\0';

    char *portmap_page = malloc(portmap_html_size + strlen(result) + strlen(ip_prefix));
    sprintf(portmap_page, portmap_start, result, ip_prefix);

    closeHeader(req);

    esp_err_t ret = httpd_resp_send(req, portmap_page, HTTPD_RESP_USE_STRLEN);
    free(portmap_page);
    ESP_LOGI(TAG, "Requesting portmap page");
    return ret;
}

esp_err_t portmap_post_handler(httpd_req_t *req)
{
    if (isLocked())
    {
        return redirectToLock(req);
    }

    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "/result");
    return httpd_resp_send(req, NULL, 0);
}
