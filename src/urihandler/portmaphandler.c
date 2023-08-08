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
const char *PORTMAP_ROW_TEMPLATE = "<tr> <td>%s</td> <td>%hu</td> <td>%s</td> <td>%hu</td> <td> <form action='/portmap' method='POST'><input type='hidden' name='entry' value='%s'> <button title='Remove' name='remove' class='btn btn-primary'> <svg version='2.0' width='16' height='16'> <use href='#trash' /> </svg> </form> </td></tr>";

esp_err_t portmap_get_handler(httpd_req_t *req)
{
    if (isLocked())
    {
        return redirectToLock(req);
    }
    ESP_LOGI(TAG, "Requesting portmap page");
    httpd_req_to_sockfd(req);

    // send first part
    extern const unsigned char portmap_start[] asm("_binary_portmap_start_html_start");
    ESP_LOGI(TAG, "Sending portmap start part");
    ESP_ERROR_CHECK(httpd_resp_send_chunk(req, (const char *)portmap_start, HTTPD_RESP_USE_STRLEN));

    // send entries
    for (int i = 0; i < PORTMAP_MAX; i++)
    {
        if (portmap_tab[i].valid)
        {
            ESP_LOGI(TAG, "%s", portmap_tab[i].proto == PROTO_TCP ? "TCP " : "UDP ");

            char *protocol;
            if (portmap_tab[i].proto == PROTO_TCP)
            {
                protocol = "TCP";
            }
            else
            {
                protocol = "UDP";
            }
            esp_ip4_addr_t addr;
            addr.addr = portmap_tab[i].daddr;
            char ip_str[16];
            sprintf(ip_str, IPSTR, IP2STR(&addr));
            char *template = malloc(strlen(PORTMAP_ROW_TEMPLATE) + 12 + strlen(ip_str) + 2 * strlen(protocol));
            sprintf(template, PORTMAP_ROW_TEMPLATE, protocol, portmap_tab[i].mport, ip_str, portmap_tab[i].dport, protocol);

            ESP_LOGI(TAG, "Sending portmap entry part: %s", template);
            ESP_ERROR_CHECK(httpd_resp_send_chunk(req, template, HTTPD_RESP_USE_STRLEN));

            free(template);

            addr.addr = my_ip;
            ESP_LOGI(TAG, IPSTR ":%d -> ", IP2STR(&addr), portmap_tab[i].mport);
            ESP_LOGI(TAG, IPSTR ":%d\n", IP2STR(&addr), portmap_tab[i].dport);
        }
    }

    // send end

    extern const char portmap_end_start[] asm("_binary_portmap_end_html_start");
    extern const char portmap_end[] asm("_binary_portmap_end_html_end");
    const size_t portmap_html_size = (portmap_end - portmap_end_start);
    char *defaultIP = getDefaultIPByNetmask();
    char ip_prefix[strlen(defaultIP) - 1];
    strncpy(ip_prefix, defaultIP, strlen(defaultIP) - 1); // Without the last part
    ip_prefix[strlen(defaultIP) - 1] = '\0';
    char *portmap_page = malloc(portmap_html_size + strlen(ip_prefix));
    sprintf(portmap_page, portmap_end_start, ip_prefix);
    ESP_LOGI(TAG, "Sending portmap end part");

    ESP_ERROR_CHECK(httpd_resp_send_chunk(req, portmap_page, HTTPD_RESP_USE_STRLEN));

    int allocatedSize = (strlen(PORTMAP_ROW_TEMPLATE) + 100); // *entries
    free(portmap_page);

    char result[allocatedSize];
    strcpy(result, "");

    // Finalize
    closeHeader(req);
    esp_err_t ret = httpd_resp_send_chunk(req, NULL, 0);

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
