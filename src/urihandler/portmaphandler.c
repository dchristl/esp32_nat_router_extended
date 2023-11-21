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
const char *PORTMAP_ROW_TEMPLATE = "<tr> <td>%s</td> <td>%hu</td> <td>%s</td> <td>%hu</td> <td> <form action='/portmap' method='POST'><input type='hidden' name='func' value='del'><input type='hidden' name='entry' value='%s'> <button title='Remove' name='remove' class='btn btn-light'> <svg version='2.0' width='16' height='16'> <use href='#trash' /> </svg> </form> </td></tr>";

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
    bool entriesSent = false;
    for (int i = 0; i < PORTMAP_MAX; i++)
    {
        if (portmap_tab[i].valid)
        {
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
            char delParam[50];
            sprintf(delParam, "%s_%hu_%s_%hu", protocol, portmap_tab[i].mport, ip_str, portmap_tab[i].dport);

            char *template = malloc(strlen(PORTMAP_ROW_TEMPLATE) + 12 + strlen(ip_str) + strlen(protocol) + strlen(delParam));

            sprintf(template, PORTMAP_ROW_TEMPLATE, protocol, portmap_tab[i].mport, ip_str, portmap_tab[i].dport, delParam);

            ESP_LOGI(TAG, "Sending portmap entry part");
            ESP_ERROR_CHECK(httpd_resp_send_chunk(req, template, HTTPD_RESP_USE_STRLEN));
            entriesSent = true;
            free(template);
        }
    }
    if (!entriesSent)
    {
        ESP_LOGI(TAG, "Sending no entry part");
        const char *template = "<tr><td colspan='5' class='text-warning'>No portmap entries found</td></tr>";
        ESP_ERROR_CHECK(httpd_resp_send_chunk(req, template, HTTPD_RESP_USE_STRLEN));
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

    free(portmap_page);
    free(defaultIP);

    // Finalize
    closeHeader(req);
    esp_err_t ret = httpd_resp_send_chunk(req, NULL, 0);

    ESP_LOGI(TAG, "Requesting portmap page");
    return ret;
}

void addPortmapEntry(char *urlContent)
{
    size_t contentLength = 64;
    char param[contentLength];
    readUrlParameterIntoBuffer(urlContent, "protocol", param, contentLength);
    uint8_t tcp_udp;
    if (strcmp(param, "tcp") == 0)
    {
        tcp_udp = PROTO_TCP;
    }
    else
    {
        tcp_udp = PROTO_UDP;
    }
    readUrlParameterIntoBuffer(urlContent, "eport", param, contentLength);
    char *endptr;
    uint16_t ext_port = (uint16_t)strtoul(param, &endptr, 10);
    if (ext_port < 1 || *endptr != '\0')
    {
        ESP_LOGW(TAG, "External port out of range");
        return;
    }

    readUrlParameterIntoBuffer(urlContent, "ip", param, contentLength);
    char *defaultIP = getDefaultIPByNetmask();
    char resultIP[strlen(defaultIP) + strlen(param)];
    strncpy(resultIP, defaultIP, strlen(defaultIP) - 1);
    resultIP[strlen(defaultIP) - 1] = '\0';
    strcat(resultIP, param);
    free(defaultIP);
    uint32_t int_ip = ipaddr_addr(resultIP);
    if (int_ip == IPADDR_NONE)
    {
        ESP_LOGW(TAG, "Invalid IP");
        return;
    }
    readUrlParameterIntoBuffer(urlContent, "iport", param, contentLength);
    uint16_t int_port = (uint16_t)strtoul(param, &endptr, 10);

    if (int_port < 1 || *endptr != '\0')
    {
        ESP_LOGW(TAG, "Internal port out of range");
        return;
    }

    add_portmap(tcp_udp, ext_port, int_ip, int_port);
}

void delPortmapEntry(char *urlContent)
{
    size_t contentLength = 64;
    char param[contentLength];
    readUrlParameterIntoBuffer(urlContent, "entry", param, contentLength);

    const char delimiter[] = "_";

    char *token = strtok(param, delimiter);
    uint8_t tcp_udp;
    if (strcmp(token, "TCP") == 0)
    {
        tcp_udp = PROTO_TCP;
    }
    else
    {
        tcp_udp = PROTO_UDP;
    }

    token = strtok(NULL, delimiter);
    char *endptr;
    uint16_t ext_port = (uint16_t)strtoul(token, &endptr, 10);
    if (ext_port < 1 || *endptr != '\0')
    {
        ESP_LOGW(TAG, "External port out of range");
        return;
    }
    token = strtok(NULL, delimiter);
    uint32_t int_ip = ipaddr_addr(token);
    if (int_ip == IPADDR_NONE)
    {
        ESP_LOGW(TAG, "Invalid IP");
        return;
    }

    token = strtok(NULL, delimiter);
    uint16_t int_port = (uint16_t)strtoul(token, &endptr, 10);

    if (int_port < 1 || *endptr != '\0')
    {
        ESP_LOGW(TAG, "Internal port out of range");
        return;
    }

    del_portmap(tcp_udp, ext_port, int_ip, int_port);
}

esp_err_t portmap_post_handler(httpd_req_t *req)
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
            addPortmapEntry(buf);
        }
        if (strcmp(funcParam, "del") == 0)
        {
            delPortmapEntry(buf);
        }
    }

    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "/portmap");
    return httpd_resp_send(req, NULL, 0);
}