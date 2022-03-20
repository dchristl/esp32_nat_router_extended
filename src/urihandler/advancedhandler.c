#include "handler.h"
#include <sys/param.h>
#include "router_globals.h"
#include "esp_wifi.h"

static const char *TAG = "Advancedhandler";

esp_err_t advanced_download_get_handler(httpd_req_t *req)
{
    if (isLocked())
    {
        return unlock_handler(req);
    }

    httpd_req_to_sockfd(req);
    extern const char advanced_start[] asm("_binary_advanced_html_start");
    extern const char advanced_end[] asm("_binary_advanced_html_end");
    const size_t advanced_html_size = (advanced_end - advanced_start);

    // char *display = NULL;

    int param_count = 4;

    int keepAlive = 0;
    int ledDisabled = 0;
    char *aliveCB = "";
    char *ledCB = "";
    char *currentDNS = "";
    char currentMAC[18];
    size_t size = param_count * 2; //%s for parameter substitution

    get_config_param_int("keep_alive", &keepAlive);
    if (keepAlive == 1)
    {
        aliveCB = "checked";
    }
    get_config_param_int("led_disabled", &ledDisabled);
    if (ledDisabled == 0)
    {
        ledCB = "checked";
    }
    ip4_addr_t usedDNS = dhcps_dns_getserver();
    currentDNS = ip4addr_ntoa(&usedDNS);

    uint8_t base_mac_addr[6] = {0};
    ESP_ERROR_CHECK(esp_efuse_mac_get_default(base_mac_addr));

    sprintf(currentMAC, "%x:%x:%x:%x:%x:%x", base_mac_addr[0], base_mac_addr[1], base_mac_addr[2], base_mac_addr[3], base_mac_addr[4], base_mac_addr[5]);

    size = size + strlen(aliveCB) + strlen(ledCB) + strlen(currentDNS) + strlen(currentMAC);
    ESP_LOGI(TAG, "Allocating additional %d bytes for advanced page.", advanced_html_size + size);
    char *advanced_page = malloc(advanced_html_size + size);
    sprintf(advanced_page, advanced_start, ledCB, aliveCB, currentDNS, currentMAC);
    closeHeader(req);
    esp_err_t ret = httpd_resp_send(req, advanced_page, strlen(advanced_page) - (param_count * 2)); // -2 for every parameter substitution (%s)
    free(advanced_page);

    return ret;
}
