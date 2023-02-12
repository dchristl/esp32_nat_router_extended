/* Console example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <pthread.h>
#include "esp_system.h"
#include "esp_log.h"
#include "esp_console.h"
#include "esp_vfs_dev.h"
#include "driver/uart.h"
#include "linenoise/linenoise.h"
#include "argtable3/argtable3.h"
#include "esp_vfs_fat.h"
#include "nvs.h"
#include "nvs_flash.h"

#include "esp_event.h"

#include "freertos/event_groups.h"
#include "esp_wifi.h"

// #include "lwip/opt.h"
#include "lwip/err.h"
#include "lwip/sys.h"

#include "cmd_decl.h"
#include <esp_http_server.h>
#include "driver/gpio.h"
#include "lwip/dns.h"
#include "esp_mac.h"

#if !IP_NAPT
#error "IP_NAPT must be defined"
#endif
#include "lwip/lwip_napt.h"

#include "router_globals.h"

// On board LED
#define BLINK_GPIO 2

#define RESET_PIN_MASK ((1ULL << GPIO_NUM_23))

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about one event
 * - are we connected to the AP with an IP? */
const int WIFI_CONNECTED_BIT = BIT0;

bool ap_connect = false;

uint32_t my_ip;
uint32_t my_ap_ip;

struct portmap_table_entry
{
    u32_t daddr;
    u16_t mport;
    u16_t dport;
    u8_t proto;
    u8_t valid;
};
struct portmap_table_entry portmap_tab[IP_PORTMAP_MAX];

esp_netif_t *wifiAP;
esp_netif_t *wifiSTA;

httpd_handle_t start_webserver(void);

static const char *TAG = "ESP32 NAT router";

/* Console command history can be stored to and loaded from a file.
 * The easiest way to do this is to use FATFS filesystem on top of
 * wear_levelling library.
 */
#if CONFIG_STORE_HISTORY

#define MOUNT_PATH "/data"
#define HISTORY_PATH MOUNT_PATH "/history.txt"

static void initialize_filesystem(void)
{
    static wl_handle_t wl_handle;
    const esp_vfs_fat_mount_config_t mount_config = {
        .max_files = 4,
        .format_if_mount_failed = true};
    esp_err_t err = esp_vfs_fat_spiflash_mount(MOUNT_PATH, "storage", &mount_config, &wl_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to mount FATFS (%s)", esp_err_to_name(err));
        return;
    }
}
#endif // CONFIG_STORE_HISTORY

static void initialize_nvs(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
}

esp_err_t apply_portmap_tab()
{
    for (int i = 0; i < IP_PORTMAP_MAX; i++)
    {
        if (portmap_tab[i].valid)
        {
            ip_portmap_add(portmap_tab[i].proto, my_ip, portmap_tab[i].mport, portmap_tab[i].daddr, portmap_tab[i].dport);
        }
    }
    return ESP_OK;
}

esp_err_t delete_portmap_tab()
{
    for (int i = 0; i < IP_PORTMAP_MAX; i++)
    {
        if (portmap_tab[i].valid)
        {
            ip_portmap_remove(portmap_tab[i].proto, portmap_tab[i].mport);
        }
    }
    return ESP_OK;
}

void print_portmap_tab()
{
    for (int i = 0; i < IP_PORTMAP_MAX; i++)
    {
        if (portmap_tab[i].valid)
        {
            printf("%s", portmap_tab[i].proto == PROTO_TCP ? "TCP " : "UDP ");
            esp_ip4_addr_t addr;
            addr.addr = my_ip;
            printf(IPSTR ":%d -> ", IP2STR(&addr), portmap_tab[i].mport);
            addr.addr = portmap_tab[i].daddr;
            printf(IPSTR ":%d\n", IP2STR(&addr), portmap_tab[i].dport);
        }
    }
}

esp_err_t get_portmap_tab()
{
    esp_err_t err;
    nvs_handle_t nvs;
    size_t len;

    err = nvs_open(PARAM_NAMESPACE, NVS_READWRITE, &nvs);
    if (err != ESP_OK)
    {
        return err;
    }
    err = nvs_get_blob(nvs, "portmap_tab", NULL, &len);
    if (err == ESP_OK)
    {
        if (len != sizeof(portmap_tab))
        {
            err = ESP_ERR_NVS_INVALID_LENGTH;
        }
        else
        {
            err = nvs_get_blob(nvs, "portmap_tab", portmap_tab, &len);
            if (err != ESP_OK)
            {
                memset(portmap_tab, 0, sizeof(portmap_tab));
            }
        }
    }
    nvs_close(nvs);

    return err;
}

esp_err_t add_portmap(u8_t proto, u16_t mport, u32_t daddr, u16_t dport)
{
    esp_err_t err;
    nvs_handle_t nvs;

    for (int i = 0; i < IP_PORTMAP_MAX; i++)
    {
        if (!portmap_tab[i].valid)
        {
            portmap_tab[i].proto = proto;
            portmap_tab[i].mport = mport;
            portmap_tab[i].daddr = daddr;
            portmap_tab[i].dport = dport;
            portmap_tab[i].valid = 1;

            err = nvs_open(PARAM_NAMESPACE, NVS_READWRITE, &nvs);
            if (err != ESP_OK)
            {
                return err;
            }
            err = nvs_set_blob(nvs, "portmap_tab", portmap_tab, sizeof(portmap_tab));
            if (err == ESP_OK)
            {
                err = nvs_commit(nvs);
                if (err == ESP_OK)
                {
                    ESP_LOGI(TAG, "New portmap table stored.");
                }
            }
            nvs_close(nvs);

            ip_portmap_add(proto, my_ip, mport, daddr, dport);

            return ESP_OK;
        }
    }
    return ESP_ERR_NO_MEM;
}

esp_err_t del_portmap(u8_t proto, u16_t mport)
{
    esp_err_t err;
    nvs_handle_t nvs;

    for (int i = 0; i < IP_PORTMAP_MAX; i++)
    {
        if (portmap_tab[i].valid && portmap_tab[i].mport == mport && portmap_tab[i].proto == proto)
        {
            portmap_tab[i].valid = 0;

            err = nvs_open(PARAM_NAMESPACE, NVS_READWRITE, &nvs);
            if (err != ESP_OK)
            {
                return err;
            }
            err = nvs_set_blob(nvs, "portmap_tab", portmap_tab, sizeof(portmap_tab));
            if (err == ESP_OK)
            {
                err = nvs_commit(nvs);
                if (err == ESP_OK)
                {
                    ESP_LOGI(TAG, "New portmap table stored.");
                }
            }
            nvs_close(nvs);

            ip_portmap_remove(proto, mport);
            return ESP_OK;
        }
    }
    return ESP_OK;
}

static void initialize_console(void)
{
    /* Drain stdout before reconfiguring it */
    fflush(stdout);
    fsync(fileno(stdout));

    /* Disable buffering on stdin */
    setvbuf(stdin, NULL, _IONBF, 0);

    /* Minicom, screen, idf_monitor send CR when ENTER key is pressed */
    esp_vfs_dev_uart_port_set_rx_line_endings(0, ESP_LINE_ENDINGS_CR);
    /* Move the caret to the beginning of the next line on '\n' */
    esp_vfs_dev_uart_port_set_tx_line_endings(0, ESP_LINE_ENDINGS_CRLF);

    /* Configure UART. Note that REF_TICK is used so that the baud rate remains
     * correct while APB frequency is changing in light sleep mode.
     */
    const uart_config_t uart_config = {.baud_rate = CONFIG_ESP_CONSOLE_UART_BAUDRATE,
                                       .data_bits = UART_DATA_8_BITS,
                                       .parity = UART_PARITY_DISABLE,
                                       .stop_bits = UART_STOP_BITS_1,
#if CONFIG_IDF_TARGET_ESP32 || CONFIG_IDF_TARGET_ESP32S2
                                       .source_clk = UART_SCLK_REF_TICK,
#else
                                       .source_clk = UART_SCLK_XTAL,
#endif
    };

    /* Install UART driver for interrupt-driven reads and writes */
    ESP_ERROR_CHECK(uart_driver_install(CONFIG_ESP_CONSOLE_UART_NUM,
                                        256, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(CONFIG_ESP_CONSOLE_UART_NUM, &uart_config));

    /* Tell VFS to use UART driver */
    esp_vfs_dev_uart_use_driver(CONFIG_ESP_CONSOLE_UART_NUM);

    /* Initialize the console */
    esp_console_config_t console_config = {.max_cmdline_args = 8,
                                           .max_cmdline_length = 256,
#if CONFIG_LOG_COLORS
                                           .hint_color = atoi(LOG_COLOR_CYAN)
#endif
    };
    ESP_ERROR_CHECK(esp_console_init(&console_config));

    /* Configure linenoise line completion library */
    /* Enable multiline editing. If not set, long commands will scroll within
     * single line.
     */
    linenoiseSetMultiLine(1);

    /* Tell linenoise where to get command completions and hints */
    linenoiseSetCompletionCallback(&esp_console_get_completion);
    linenoiseSetHintsCallback((linenoiseHintsCallback *)&esp_console_get_hint);

    /* Set command history size */
    linenoiseHistorySetMaxLen(100);

#if CONFIG_STORE_HISTORY
    /* Load command history from filesystem */
    linenoiseHistoryLoad(HISTORY_PATH);
#endif
}

void *led_status_thread(void *p)
{
    gpio_reset_pin(BLINK_GPIO);
    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);

    uint16_t connect_count = getConnectCount();

    while (true)
    {
        gpio_set_level(BLINK_GPIO, ap_connect);

        for (int i = 0; i < connect_count; i++)
        {
            gpio_set_level(BLINK_GPIO, 1 - ap_connect);
            vTaskDelay(50 / portTICK_PERIOD_MS);
            gpio_set_level(BLINK_GPIO, ap_connect);
            vTaskDelay(50 / portTICK_PERIOD_MS);
        }

        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

void fillDNS(ip_addr_t *dnsserver)
{
    char *customDNS = NULL;
    get_config_param_str("custom_dns", &customDNS);

    if (customDNS == NULL)
    {
        // If can't get DNS server, uses default one
        if (dnsserver->type == IPADDR_ANY)
        {
            dnsserver->u_addr.ip4.addr = esp_ip4addr_aton(getDefaultIPByNetmask());
        }
    }
    else
    {
        ESP_LOGI(TAG, "Setting custom DNS server to: %s", customDNS);
        dnsserver->u_addr.ip4.addr = esp_ip4addr_aton(customDNS);
    }
}

void fillMac()
{
    char *customMac = NULL;
    get_config_param_str("custom_mac", &customMac);
    if (customMac != NULL)
    {

        if (strcmp("random", customMac) == 0)
        {
            uint8_t default_mac_addr[6] = {0};
            ESP_ERROR_CHECK(esp_efuse_mac_get_default(default_mac_addr));
            default_mac_addr[5] = (esp_random() % 254 + 1);
            ESP_LOGI(TAG, "Setting random MAC address: %x:%x:%x:%x:%x:%x", default_mac_addr[0], default_mac_addr[1], default_mac_addr[2], default_mac_addr[3], default_mac_addr[4], default_mac_addr[5]);
            esp_base_mac_addr_set(default_mac_addr);
        }
        else
        {
            uint8_t uintMacs[6] = {0};
            ESP_LOGI(TAG, "Setting custom MAC address: %s", customMac);
            int intMacs[6] = {0};

            sscanf(customMac, "%x:%x:%x:%x:%x:%x", &intMacs[0], &intMacs[1], &intMacs[2], &intMacs[3], &intMacs[4], &intMacs[5]);
            for (int i = 0; i < 6; ++i)
            {
                uintMacs[i] = intMacs[i];
            }
            esp_base_mac_addr_set(uintMacs);
        }
    }
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    esp_netif_dns_info_t dns;
    ip_addr_t dnsserver;

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        ESP_LOGI(TAG, "disconnected - retry to connect to the AP");
        ap_connect = false;
        esp_wifi_connect();
        ESP_LOGI(TAG, "retry to connect to the AP");
        xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT);
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        stop_dns_server();
        if (esp_netif_get_dns_info(wifiSTA, ESP_NETIF_DNS_MAIN, &dns) == ESP_OK)
        {
            
            dnsserver.type = IPADDR_TYPE_V4;
            dnsserver.u_addr.ip4.addr = dns.ip.u_addr.ip4.addr;
            fillDNS(&dnsserver);
            dns_setserver(0, &dnsserver);
            // ESP_LOGI(TAG, "set dns to: " IPSTR, IP2STR(&(dnsserver.u_addr.ip4.addr))); FIXME
        }
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED)
    {
        ESP_LOGI(TAG, "Station connected");
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STADISCONNECTED)
    {
        ESP_LOGI(TAG, "Station disconnected");
    }
}
const int CONNECTED_BIT = BIT0;
#define JOIN_TIMEOUT_MS (2000)

void wifi_init(const char *ssid, const char *passwd, const char *static_ip, const char *subnet_mask, const char *gateway_addr, const char *ap_ssid, const char *ap_passwd, const char *ap_ip)
{
    esp_ip4_addr_t dnsserver;
    // tcpip_adapter_dns_info_t dnsinfo;

    wifi_event_group = xEventGroupCreate();

    esp_netif_init();
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifiAP = esp_netif_create_default_wifi_ap();
    wifiSTA = esp_netif_create_default_wifi_sta();

    esp_netif_ip_info_t ipInfo_sta;
    if ((strlen(ssid) > 0) && (strlen(static_ip) > 0) && (strlen(subnet_mask) > 0) && (strlen(gateway_addr) > 0))
    {
        my_ip = ipInfo_sta.ip.addr = ipaddr_addr(static_ip);
        ipInfo_sta.gw.addr = ipaddr_addr(gateway_addr);
        ipInfo_sta.netmask.addr = ipaddr_addr(subnet_mask);
        esp_netif_dhcpc_stop(wifiSTA); // Don't run a DHCP client
        esp_netif_set_ip_info(wifiSTA, &ipInfo_sta);
        apply_portmap_tab();
    }

    my_ap_ip = ipaddr_addr(ap_ip);

    esp_netif_ip_info_t ipInfo_ap;
    ipInfo_ap.ip.addr = my_ap_ip;
    ipInfo_ap.gw.addr = my_ap_ip;

    char *netmask = getNetmask();

    ipInfo_ap.netmask.addr = ipaddr_addr(netmask);

    ESP_LOGI(TAG, "Netmask is set to %s, therefore private IP is %s", netmask, ap_ip);

    esp_netif_dhcps_stop(wifiAP); // stop before setting ip WifiAP
    esp_netif_set_ip_info(wifiAP, &ipInfo_ap);
    esp_netif_dhcps_start(wifiAP);

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    /* ESP WIFI CONFIG */
    wifi_config_t wifi_config = {0};
    wifi_config_t ap_config = {
        .ap = {
            .authmode = WIFI_AUTH_WPA2_PSK,
            .ssid_hidden = 0,
            // .channel =
            .max_connection = 10,
            .beacon_interval = 100,
            .pairwise_cipher = WIFI_CIPHER_TYPE_CCMP}};

    strlcpy((char *)ap_config.sta.ssid, ap_ssid, sizeof(ap_config.sta.ssid));
    if (strlen(ap_passwd) < 8)
    {
        ap_config.ap.authmode = WIFI_AUTH_OPEN;
    }
    else
    {
        strlcpy((char *)ap_config.sta.password, ap_passwd, sizeof(ap_config.sta.password));
    }

    if (strlen(ssid) > 0)
    {
        strlcpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
        strlcpy((char *)wifi_config.sta.password, passwd, sizeof(wifi_config.sta.password));
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
        ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &ap_config));
        ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    }
    else
    {
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
        ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &ap_config));
    }

    // Enable DNS (offer) for dhcp server
    // dhcps_offer_t dhcps_dns_value = OFFER_DNS;
    // dhcps_set_option_info(6, &dhcps_dns_value, sizeof(dhcps_dns_value));

    // Set custom dns server address for dhcp server
    dnsserver.addr = esp_ip4addr_aton(getDefaultIPByNetmask());
    // dnsserver.type = IPADDR_TYPE_V4;
    dns_setserver(0, &dnsserver);

    xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT,
                        pdFALSE, pdTRUE, JOIN_TIMEOUT_MS / portTICK_PERIOD_MS);
    ESP_ERROR_CHECK(esp_wifi_start());

    if (strlen(ssid) > 0)
    {
        ESP_LOGI(TAG, "wifi_init_apsta finished.");
        ESP_LOGI(TAG, "connect to ap SSID: %s ", ssid);
    }
    else
    {
        ESP_LOGI(TAG, "wifi_init_ap with default finished.");
    }
    start_dns_server();
    // ESP_ERROR_CHECK(esp_wifi_set_max_tx_power(80));
    // int8_t j;
    // ESP_ERROR_CHECK(esp_wifi_get_max_tx_power(&j));
    // ESP_LOGI(TAG, "\t\t\tWifi power get is %d.", j);
}

char *ssid = NULL;
char *passwd = NULL;
char *static_ip = NULL;
char *subnet_mask = NULL;
char *gateway_addr = NULL;
char *ap_ssid = NULL;
char *lock_pass = NULL;
int led_disabled = 0;
char *scan_result = NULL;
char *ap_passwd = NULL;
char *ap_ip = NULL;

char *param_set_default(const char *def_val)
{
    char *retval = malloc(strlen(def_val) + 1);
    strcpy(retval, def_val);
    return retval;
}

bool checkForResetPinAndReset()
{
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = RESET_PIN_MASK;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    ESP_ERROR_CHECK(gpio_config(&io_conf));

    int gpioLevel = gpio_get_level(GPIO_NUM_23);
    int counter = 0;
    while (counter < 5 && gpioLevel == 0)
    {
        gpio_reset_pin(BLINK_GPIO);
        gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);

        gpio_set_level(BLINK_GPIO, 1);

        for (int i = 0; i < 10; i++)
        {
            gpio_set_level(BLINK_GPIO, 0);
            vTaskDelay(50 / portTICK_PERIOD_MS);
            gpio_set_level(BLINK_GPIO, 1);
            vTaskDelay(50 / portTICK_PERIOD_MS);
        }

        counter++;
        ESP_LOGW(TAG, "Reset Pin (GPIO 23) set for %ds", counter);
        gpioLevel = gpio_get_level(GPIO_NUM_23);
    }
    if (counter == 5)
    {
        ESP_LOGW(TAG, "Device will be resetted! Disconnect bridge and reboot device afterwards.");
        int argc = 2;
        char *argv[argc];
        argv[0] = "erase_namespace";
        argv[1] = PARAM_NAMESPACE;
        erase_ns(argc, argv);
        return true;
    }
    else
    {
        gpio_set_level(BLINK_GPIO, 0);
        ESP_LOGI(TAG, "Device will boot up normally.");
    }
    return false;
}

void app_main(void)
{
    initialize_nvs();
    register_nvs();
    if (checkForResetPinAndReset())
    {
        return;
    }
    initialize_console();
    /* Register commands */
    esp_console_register_help_command();
    register_system();

    register_router();

#if CONFIG_STORE_HISTORY
    initialize_filesystem();
    ESP_LOGI(TAG, "Command history enabled");
#else
    ESP_LOGI(TAG, "Command history disabled");
#endif

    fillMac();
    get_config_param_str("ssid", &ssid);
    if (ssid == NULL)
    {
        ssid = param_set_default("");
    }
    get_config_param_str("passwd", &passwd);
    if (passwd == NULL)
    {
        passwd = param_set_default("");
    }
    get_config_param_str("static_ip", &static_ip);
    if (static_ip == NULL)
    {
        static_ip = param_set_default("");
    }
    get_config_param_str("subnet_mask", &subnet_mask);
    if (subnet_mask == NULL)
    {
        subnet_mask = param_set_default("");
    }
    get_config_param_str("gateway_addr", &gateway_addr);
    if (gateway_addr == NULL)
    {
        gateway_addr = param_set_default("");
    }
    get_config_param_str("ap_ssid", &ap_ssid);
    if (ap_ssid == NULL)
    {
        ap_ssid = param_set_default("ESP32_NAT_Router");
    }
    get_config_param_str("ap_passwd", &ap_passwd);
    if (ap_passwd == NULL)
    {
        ap_passwd = param_set_default("");
    }
    get_config_param_str("ap_ip", &ap_ip);
    if (ap_ip == NULL)
    {
        ap_ip = param_set_default(getDefaultIPByNetmask());
    }

    get_config_param_str("lock_pass", &lock_pass);
    if (lock_pass == NULL)
    {
        lock_pass = param_set_default("");
    }

    get_config_param_str("scan_result", &scan_result);
    if (scan_result == NULL)
    {
        scan_result = param_set_default("");
    }

    get_portmap_tab();

    // Setup WIFI
    wifi_init(ssid, passwd, static_ip, subnet_mask, gateway_addr, ap_ssid, ap_passwd, ap_ip);

    pthread_t t1;
    get_config_param_int("led_disabled", &led_disabled);
    if (scan_result == NULL || led_disabled == 0)
    {
        ESP_LOGI(TAG, "On board LED is enabled");
        pthread_create(&t1, NULL, led_status_thread, NULL);
    }
    else
    {
        ESP_LOGI(TAG, "On board LED is disabled");
    }

    ip_napt_enable(my_ap_ip, 1);
    ESP_LOGI(TAG, "NAT is enabled");

    int lock = 0;
    get_config_param_int("lock", &lock);
    if (lock == 0)
    {
        ESP_LOGI(TAG, "Starting config web server");
        start_webserver();
    }
    else
    {
        ESP_LOGW(TAG, "Web server is disabled. Reenable with following commands and reboot afterwards:");
        ESP_LOGW(TAG, "'nvs_namespace esp32_nat'");
        ESP_LOGW(TAG, "'nvs_set lock i32 -v 0'");
    }

    /* Prompt to be printed before each line.
     * This can be customized, made dynamic, etc.
     */
    const char *prompt = LOG_COLOR_I "esp32> " LOG_RESET_COLOR;

    printf("\n"
           "ESP32 NAT ROUTER\n"
           "Type 'help' to get the list of commands.\n"
           "Use UP/DOWN arrows to navigate through command history.\n"
           "Press TAB when typing command name to auto-complete.\n");

    if (strlen(ssid) == 0)
    {
        printf("\n"
               "Unconfigured WiFi\n"
               "Configure using 'set_sta' and 'set_ap' and restart.\n");
    }

    /* Figure out if the terminal supports escape sequences */
    int probe_status = linenoiseProbe();
    if (probe_status)
    { /* zero indicates success */
        printf("\n"
               "Your terminal application does not support escape sequences.\n"
               "Line editing and history features are disabled.\n"
               "On Windows, try using Putty instead.\n");
        linenoiseSetDumbMode(1);
#if CONFIG_LOG_COLORS
        /* Since the terminal doesn't support escape sequences,
         * don't use color codes in the prompt.
         */
        prompt = "esp32> ";
#endif // CONFIG_LOG_COLORS
    }

    /* Main loop */
    while (true)
    {
        /* Get a line using linenoise.
         * The line is returned when ENTER is pressed.
         */
        char *line = linenoise(prompt);
        if (line == NULL)
        { /* Ignore empty lines */
            continue;
        }
        /* Add the command to the history */
        linenoiseHistoryAdd(line);
#if CONFIG_STORE_HISTORY
        /* Save command history to filesystem */
        linenoiseHistorySave(HISTORY_PATH);
#endif

        /* Try to run the command */
        int ret;
        esp_err_t err = esp_console_run(line, &ret);
        if (err == ESP_ERR_NOT_FOUND)
        {
            printf("Unrecognized command\n");
        }
        else if (err == ESP_ERR_INVALID_ARG)
        {
            // command was empty
        }
        else if (err == ESP_OK && ret != ESP_OK)
        {
            printf("Command returned non-zero error code: 0x%x (%s)\n", ret, esp_err_to_name(ret));
        }
        else if (err != ESP_OK)
        {
            printf("Internal error: %s\n", esp_err_to_name(err));
        }
        /* linenoise allocates line buffer on the heap, so need to free it */
        linenoiseFree(line);
    }
}
