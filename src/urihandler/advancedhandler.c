#include "handler.h"
#include <sys/param.h>
#include "dhcpserver/dhcpserver.h"
#include "router_globals.h"
#include "esp_wifi.h"
#include "esp_mac.h"
#include "esp_err.h"

static const char *TAG = "Advancedhandler";

esp_err_t advanced_download_get_handler(httpd_req_t *req)
{
    if (isLocked())
    {
        return redirectToLock(req);
    }

    httpd_req_to_sockfd(req);
    extern const char advanced_start[] asm("_binary_advanced_html_start");
    extern const char advanced_end[] asm("_binary_advanced_html_end");
    const size_t advanced_html_size = (advanced_end - advanced_start);

    int32_t keepAlive = 0;
    int32_t ledDisabled = 0;
    int32_t natDisabled = 0;
    char *aliveCB = "";
    char *ledCB = "";
    char *natCB = "";
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
    char *classACB = "";
    char *classBCB = "";
    char *classCCB = "";

    char currentMAC[18];
    char defaultMAC[18];

    char *hostName = NULL;
    get_config_param_str("hostname", &hostName);

    int32_t txPower = 0;
    get_config_param_int("txpower", &txPower);
    if (txPower < 8 || txPower > 84)
    {
        txPower = 80; // default
    }
    char *lowSelected, *mediumSelected, *highSelected = NULL;
    if (txPower < 34)
    { // low
        lowSelected = "selected";
        mediumSelected = "";
        highSelected = "";
    }
    else if (txPower < 60)
    { // medium
        lowSelected = "";
        mediumSelected = "selected";
        highSelected = "";
    }
    else
    { // high
        lowSelected = "";
        mediumSelected = "";
        highSelected = "selected";
    }

    int32_t bandwith = 0;
    get_config_param_int("lower_bandwith", &bandwith);
    char *bwLow, *bwHigh = NULL;
    if (bandwith == 1)
    {
        bwLow = "selected";
        bwHigh = "";
    }
    else
    {
        bwLow = "";
        bwHigh = "selected";
    }

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
    get_config_param_int("nat_disabled", &natDisabled);
    if (natDisabled == 0)
    {
        natCB = "checked";
    }
    esp_netif_dns_info_t dns;
    esp_netif_t *wifiSTA = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    if (esp_netif_get_dns_info(wifiSTA, ESP_NETIF_DNS_MAIN, &dns) == ESP_OK)
    {
        currentDNS = malloc(16);
        sprintf(currentDNS, IPSTR, IP2STR(&(dns.ip.u_addr.ip4)));
        ESP_LOGI(TAG, "Current DNS is: %s", currentDNS);
    }

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

    const char *netmask = getNetmask();

    if (strcmp(netmask, DEFAULT_NETMASK_CLASS_A) == 0)
    {
        classACB = "checked";
    }

    else if (strcmp(netmask, DEFAULT_NETMASK_CLASS_B) == 0)
    {
        classBCB = "checked";
    }
    else
    {
        classCCB = "checked";
    }

    u_int size = advanced_html_size + strlen(aliveCB) + strlen(ledCB) + strlen(natCB) + strlen(currentDNS) + strlen(currentMAC) + 3 * strlen("checked") + strlen(customDNSIP) + 2 * strlen(defaultMAC) + strlen(customMac) + strlen(netmask) + strlen(hostName) + 2 * strlen("selected");
    ESP_LOGI(TAG, "Allocating additional %d bytes for advanced page.", size);
    char *advanced_page = malloc(size);

    char *subMac = malloc(strlen(defaultMAC) + 1);
    strcpy(subMac, defaultMAC);

    subMac[strlen(subMac) - 2] = '\0';

    sprintf(advanced_page, advanced_start, hostName, lowSelected, mediumSelected, highSelected, bwHigh, bwLow, ledCB, aliveCB, natCB, currentDNS, defCB, cloudCB, adguardCB, customCB, customDNSIP, currentMAC, defMacCB, defaultMAC, rndMacCB, subMac, customMacCB, customMac, netmask, classCCB, classBCB, classACB);

    closeHeader(req);
    esp_err_t ret = httpd_resp_send(req, advanced_page, HTTPD_RESP_USE_STRLEN);

    free(advanced_page);
    free(subMac);
    free(currentDNS);

    return ret;
}
