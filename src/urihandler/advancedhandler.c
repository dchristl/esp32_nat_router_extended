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

    int keepAlive = 0;
    int ledDisabled = 0;
    char *aliveCB = "";
    char *ledCB = "";
    char *currentDNS = "";
    char *defCB = "";
    char *cloudCB = "";
    char *adguardCB = "";
    char *customCB = "";
    char *customDNSIP = "";
    char *defMacCB = "";
    char *rndMacCB = "";
    char *customMacCB = "";
    char *customMac = "";
    char *macSetting = "";

    char currentMAC[18];
    char defaultMAC[18];

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

    char *customDNS = NULL;
    get_config_param_str("custom_dns", &customDNS);

    if (customDNS == NULL)
    {
        defCB = "checked";
    }
    else if (strcmp(customDNS, "1.1.1.1") == 0)
    {
        cloudCB = "checked";
    }
    else if (strcmp(customDNS, "94.140.14.14") == 0)
    {
        adguardCB = "checked";
    }
    else
    {
        customCB = "checked";
        get_config_param_str("custom_dns", &customDNSIP);
    }

    uint8_t base_mac_addr[6] = {0};
    uint8_t default_mac_addr[6] = {0};
    ESP_ERROR_CHECK(esp_base_mac_addr_get(base_mac_addr));
    ESP_ERROR_CHECK(esp_efuse_mac_get_default(default_mac_addr));

    sprintf(currentMAC, "%x:%x:%x:%x:%x:%x", base_mac_addr[0], base_mac_addr[1], base_mac_addr[2], base_mac_addr[3], base_mac_addr[4], base_mac_addr[5]);
    sprintf(defaultMAC, "%x:%x:%x:%x:%x:%x", default_mac_addr[0], default_mac_addr[1], default_mac_addr[2], default_mac_addr[3], default_mac_addr[4], default_mac_addr[5]);

    get_config_param_str("custom_mac", &macSetting);

    if (strcmp(macSetting, "random") == 0)
    {
        rndMacCB = "checked";
    }

    else if (strcmp(currentMAC, defaultMAC) == 0)
    {
        defMacCB = "checked";
    }
    else
    {
        customMacCB = "checked";
        customMac = currentMAC;
    }

    u_int size = advanced_html_size + strlen(aliveCB) + strlen(ledCB) + strlen(currentDNS) + strlen(currentMAC) + 2 * strlen("checked") + strlen(customDNSIP) + 2 * strlen(defaultMAC) + strlen(customMac);
    ESP_LOGI(TAG, "Allocating additional %d bytes for advanced page.", size);
    char *advanced_page = malloc(size);

    char *subMac = malloc(strlen(defaultMAC) + 1);
    strcpy(subMac, defaultMAC);

    subMac[strlen(subMac) - 2] = '\0';

    sprintf(advanced_page, advanced_start, ledCB, aliveCB, currentDNS, defCB, cloudCB, adguardCB, customCB, customDNSIP, currentMAC, defMacCB, defaultMAC, rndMacCB, subMac, customMacCB, customMac);

    closeHeader(req);
    esp_err_t ret = httpd_resp_send(req, advanced_page, HTTPD_RESP_USE_STRLEN);

    free(advanced_page);
    free(subMac);

    return ret;
}
