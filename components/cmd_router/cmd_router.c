/* The CLI commands of the router

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "esp_log.h"
#include "esp_console.h"
#include "esp_system.h"
#include "esp_sleep.h"
#include "spi_flash_mmap.h"
#include "driver/rtc_io.h"
#include "driver/uart.h"
#include "argtable3/argtable3.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include "nvs.h"
#include "esp_wifi.h"
#include "lwip/ip4_addr.h"
#if !IP_NAPT
#error "IP_NAPT must be defined"
#endif
#include "lwip/lwip_napt.h"

#include "router_globals.h"
#include "cmd_router.h"

#ifdef CONFIG_FREERTOS_USE_STATS_FORMATTING_FUNCTIONS
#define WITH_TASKS_INFO 1
#endif

static const char *TAG = "cmd_router";

static void register_set_sta(void);
static void register_set_sta_ent(void);
static void register_set_sta_static(void);
static void register_set_ap(void);
static void register_set_ap_ip(void);
static void register_show(void);
static void register_portmap(void);
static void register_static_ip(void);

esp_err_t get_config_param_str(char *name, char **param)
{
    nvs_handle_t nvs;

    esp_err_t err = nvs_open(PARAM_NAMESPACE, NVS_READONLY, &nvs);
    if (err == ESP_OK)
    {
        size_t len;
        if ((err = nvs_get_str(nvs, name, NULL, &len)) == ESP_OK)
        {
            *param = (char *)malloc(len);
            err = nvs_get_str(nvs, name, *param, &len);
            ESP_LOGI(TAG, "%s %s", name, *param);
        }
        else
        {
            return err;
        }
        nvs_close(nvs);
    }
    else
    {
        return err;
    }
    return ESP_OK;
}

esp_err_t get_config_param_blob(char *name, char **param, size_t *blob_len)
{
    nvs_handle_t nvs;

    esp_err_t err = nvs_open(PARAM_NAMESPACE, NVS_READONLY, &nvs);
    if (err == ESP_OK)
    {
        size_t len;
        if ((err = nvs_get_blob(nvs, name, NULL, &len)) == ESP_OK)
        {
            *param = (char *)malloc(len);
            *blob_len = len;

            err = nvs_get_blob(nvs, name, *param, &len);
        }
        else
        {
            return err;
        }
        nvs_close(nvs);
    }
    else
    {
        return err;
    }
    return ESP_OK;
}

esp_err_t get_config_param_blob2(char *name, uint8_t *blob, size_t blob_len)
{
    nvs_handle_t nvs;

    esp_err_t err = nvs_open(PARAM_NAMESPACE, NVS_READONLY, &nvs);
    if (err == ESP_OK)
    {
        size_t len;
        if ((err = nvs_get_blob(nvs, name, NULL, &len)) == ESP_OK)
        {
            if (len != blob_len)
            {
                return ESP_ERR_NVS_INVALID_LENGTH;
            }
            err = nvs_get_blob(nvs, name, blob, &len);
            ESP_LOGD(TAG, "%s: %d", name, len);
        }
        else
        {
            return err;
        }
        nvs_close(nvs);
    }
    else
    {
        return err;
    }
    return ESP_OK;
}

esp_err_t get_config_param_int(char *name, int32_t *param)
{
    nvs_handle_t nvs;

    esp_err_t err = nvs_open(PARAM_NAMESPACE, NVS_READONLY, &nvs);
    if (err == ESP_OK)
    {
        if ((err = nvs_get_i32(nvs, name, param)) == ESP_OK)
        {
            ESP_LOGD(TAG, "%s %ld", name, *param);
        }
        else
        {
            return err;
        }
        nvs_close(nvs);
    }
    else
    {
        return err;
    }
    return ESP_OK;
}

esp_err_t erase_key(char *name)
{
    nvs_handle_t nvs;

    esp_err_t err = nvs_open(PARAM_NAMESPACE, NVS_READWRITE, &nvs);
    if (err == ESP_OK)
    {
        nvs_erase_key(nvs, name);
        nvs_commit(nvs);
        nvs_close(nvs);
        return ESP_OK;
    }
    else
    {
        return err;
    }
    return ESP_OK;
}

char *getDefaultIPByNetmask()
{
    char *result = (char *)malloc(15);
    char *netmask = getNetmask();

    int32_t octet = 4;
    get_config_param_int("octet", &octet);

    char *netmask_to_compare = "255.255.255.";
    if (strncmp(netmask, netmask_to_compare, strlen(netmask_to_compare)) == 0)
    {
        sprintf(result, DEFAULT_AP_IP_CLASS_C, octet);
        return result;
    }

    netmask_to_compare = "255.255.";
    if (strncmp(netmask, netmask_to_compare, strlen(netmask_to_compare)) == 0)
    {

        sprintf(result, DEFAULT_AP_IP_CLASS_B, octet);
        return result;
    }
    else
    {
        sprintf(result, DEFAULT_AP_IP_CLASS_A, octet);
        return result;
    }
}

char *getNetmask()
{
    char *netmask = NULL;

    get_config_param_str("netmask", &netmask);

    if (netmask == NULL)
    {
        return DEFAULT_NETMASK_CLASS_C;
    }
    else
    {
        return netmask;
    }
}

void cleanConsoleString(char *str)
{
    char *p, *q;

    for (p = q = str; *p != 0; p++)
    {
        if (*(p) == '%' && *(p + 1) != 0 && *(p + 2) != 0)
        {
            // quoted hex
            uint8_t a;
            p++;
            if (*p <= '9')
                a = *p - '0';
            else
                a = toupper((unsigned char)*p) - 'A' + 10;
            a <<= 4;
            p++;
            if (*p <= '9')
                a += *p - '0';
            else
                a += toupper((unsigned char)*p) - 'A' + 10;
            *q++ = a;
        }
        else if (*(p) == '+')
        {
            *q++ = ' ';
        }
        else
        {
            *q++ = *p;
        }
    }
    *q = '\0';
}
void register_router(void)
{
    register_set_sta();
    register_set_sta_static();
    register_set_ap();
    register_set_sta_ent();
    register_set_ap_ip();
    register_portmap();
    register_show();
    register_static_ip();
}

/** Arguments used by 'set_sta' function */
static struct
{
    struct arg_str *ssid;
    struct arg_str *password;
    struct arg_end *end;
} set_sta_arg;

/** Arguments used by 'set_sta_ent' function */
static struct
{
    struct arg_str *ssid;
    struct arg_str *identity;
    struct arg_str *user;
    struct arg_str *password;
    struct arg_end *end;
} set_sta_ent_arg;

/* 'set_sta' command */
int set_sta(int argc, char **argv)
{
    nvs_handle_t nvs;

    int nerrors = arg_parse(argc, argv, (void **)&set_sta_arg);
    if (nerrors != 0)
    {
        arg_print_errors(stderr, set_sta_arg.end, argv[0]);
        ESP_LOGE(TAG, "Error");
        return ESP_FAIL;
    }

    cleanConsoleString((char *)set_sta_arg.ssid->sval[0]);
    cleanConsoleString((char *)set_sta_arg.password->sval[0]);

    ESP_ERROR_CHECK(nvs_open(PARAM_NAMESPACE, NVS_READWRITE, &nvs));
    ESP_ERROR_CHECK(nvs_set_str(nvs, "ssid", set_sta_arg.ssid->sval[0]));
    ESP_ERROR_CHECK(nvs_set_str(nvs, "passwd", set_sta_arg.password->sval[0]));
    // remove all WPA enterprise keys
    nvs_erase_key(nvs, "cer");
    nvs_erase_key(nvs, "sta_user");
    nvs_erase_key(nvs, "sta_identity");

    ESP_ERROR_CHECK(nvs_commit(nvs));
    ESP_LOGI(TAG, "STA settings %s/%s stored.", set_sta_arg.ssid->sval[0], set_sta_arg.password->sval[0]);

    nvs_close(nvs);
    return ESP_OK;
}

int set_sta_ent(int argc, char **argv)
{
    nvs_handle_t nvs;

    int nerrors = arg_parse(argc, argv, (void **)&set_sta_ent_arg);
    if (nerrors != 0)
    {
        arg_print_errors(stderr, set_sta_ent_arg.end, argv[0]);
        ESP_LOGE(TAG, "Error");
        return ESP_FAIL;
    }

    cleanConsoleString((char *)set_sta_ent_arg.ssid->sval[0]);
    cleanConsoleString((char *)set_sta_ent_arg.identity->sval[0]);
    cleanConsoleString((char *)set_sta_ent_arg.user->sval[0]);
    cleanConsoleString((char *)set_sta_ent_arg.password->sval[0]);

    ESP_ERROR_CHECK(nvs_open(PARAM_NAMESPACE, NVS_READWRITE, &nvs));
    ESP_ERROR_CHECK(nvs_set_str(nvs, "ssid", set_sta_ent_arg.ssid->sval[0]));
    ESP_ERROR_CHECK(nvs_set_str(nvs, "passwd", set_sta_ent_arg.password->sval[0]));
    ESP_ERROR_CHECK(nvs_set_str(nvs, "sta_user", set_sta_ent_arg.user->sval[0]));
    ESP_ERROR_CHECK(nvs_set_str(nvs, "sta_identity", set_sta_ent_arg.identity->sval[0]));

    ESP_ERROR_CHECK(nvs_commit(nvs));
    ESP_LOGI(TAG, "WPA Enterprise settings SSID: '%s', User: %s, Identity: %s, Password: %s stored.", set_sta_ent_arg.ssid->sval[0], set_sta_ent_arg.user->sval[0], set_sta_ent_arg.identity->sval[0], set_sta_ent_arg.password->sval[0]);

    nvs_close(nvs);
    return ESP_OK;
}

static void register_set_sta(void)
{
    set_sta_arg.ssid = arg_str1(NULL, NULL, "<ssid>", "SSID");
    set_sta_arg.password = arg_str1(NULL, NULL, "<passwd>", "Password");
    set_sta_arg.end = arg_end(2);

    const esp_console_cmd_t cmd = {
        .command = "set_sta",
        .help = "Set SSID and password of the STA interface",
        .hint = NULL,
        .func = &set_sta,
        .argtable = &set_sta_arg};
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}

static void register_set_sta_ent(void)
{
    set_sta_ent_arg.ssid = arg_str1(NULL, NULL, "<ssid>", "SSID");
    set_sta_ent_arg.identity = arg_str1(NULL, NULL, "<identity>", "Identity");
    set_sta_ent_arg.user = arg_str1(NULL, NULL, "<user>", "User");
    set_sta_ent_arg.password = arg_str1(NULL, NULL, "<passwd>", "Password");
    set_sta_ent_arg.end = arg_end(4);

    const esp_console_cmd_t cmd = {
        .command = "set_sta_ent",
        .help = "Set up WPA Enterprise in STA mode",
        .hint = NULL,
        .func = &set_sta_ent,
        .argtable = &set_sta_ent_arg};
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}

/** Arguments used by 'set_sta_static' function */
static struct
{
    struct arg_str *static_ip;
    struct arg_str *subnet_mask;
    struct arg_str *gateway_addr;
    struct arg_end *end;
} set_sta_static_arg;

/* 'set_sta_static' command */
int set_sta_static(int argc, char **argv)
{
    nvs_handle_t nvs;

    int nerrors = arg_parse(argc, argv, (void **)&set_sta_static_arg);
    if (nerrors != 0)
    {
        arg_print_errors(stderr, set_sta_static_arg.end, argv[0]);
        return ESP_FAIL;
    }
    cleanConsoleString((char *)set_sta_static_arg.static_ip->sval[0]);
    cleanConsoleString((char *)set_sta_static_arg.subnet_mask->sval[0]);
    cleanConsoleString((char *)set_sta_static_arg.gateway_addr->sval[0]);

    ESP_ERROR_CHECK(nvs_open(PARAM_NAMESPACE, NVS_READWRITE, &nvs));
    ESP_ERROR_CHECK(nvs_set_str(nvs, "static_ip", set_sta_static_arg.static_ip->sval[0]));
    ESP_ERROR_CHECK(nvs_set_str(nvs, "subnet_mask", set_sta_static_arg.subnet_mask->sval[0]));
    ESP_ERROR_CHECK(nvs_set_str(nvs, "gateway_addr", set_sta_static_arg.gateway_addr->sval[0]));
    ESP_ERROR_CHECK(nvs_commit(nvs));
    ESP_LOGI(TAG, "STA Static IP settings %s/%s/%s stored.", set_sta_static_arg.static_ip->sval[0], set_sta_static_arg.subnet_mask->sval[0], set_sta_static_arg.gateway_addr->sval[0]);

    nvs_close(nvs);
    return ESP_OK;
}

static void register_set_sta_static(void)
{
    set_sta_static_arg.static_ip = arg_str1(NULL, NULL, "<ip>", "IP");
    set_sta_static_arg.subnet_mask = arg_str1(NULL, NULL, "<subnet>", "Subnet Mask");
    set_sta_static_arg.gateway_addr = arg_str1(NULL, NULL, "<gw>", "Gateway Address");
    set_sta_static_arg.end = arg_end(3);

    const esp_console_cmd_t cmd = {
        .command = "set_sta_static",
        .help = "Set Static IP for the STA interface",
        .hint = NULL,
        .func = &set_sta_static,
        .argtable = &set_sta_static_arg};
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}

/** Arguments used by 'set_ap' function */
static struct
{
    struct arg_str *ssid;
    struct arg_str *password;
    struct arg_end *end;
} set_ap_args;

/* 'set_ap' command */
int set_ap(int argc, char **argv)
{
    nvs_handle_t nvs;

    int nerrors = arg_parse(argc, argv, (void **)&set_ap_args);
    if (nerrors != 0)
    {
        arg_print_errors(stderr, set_ap_args.end, argv[0]);
        return ESP_FAIL;
    }

    cleanConsoleString((char *)set_ap_args.ssid->sval[0]);
    cleanConsoleString((char *)set_ap_args.password->sval[0]);

    if (strlen(set_ap_args.password->sval[0]) < 8)
    {
        ESP_LOGW(TAG, "AP will be open (no passwd needed).\n");
    }

    ESP_ERROR_CHECK(nvs_open(PARAM_NAMESPACE, NVS_READWRITE, &nvs));
    ESP_ERROR_CHECK(nvs_set_str(nvs, "ap_ssid", set_ap_args.ssid->sval[0]));
    ESP_ERROR_CHECK(nvs_set_str(nvs, "ap_passwd", set_ap_args.password->sval[0]));
    ESP_ERROR_CHECK(nvs_commit(nvs));
    ESP_LOGI(TAG, "AP settings %s/%s stored.", set_ap_args.ssid->sval[0], set_ap_args.password->sval[0]);

    nvs_close(nvs);
    return ESP_OK;
}

static void register_set_ap(void)
{
    set_ap_args.ssid = arg_str1(NULL, NULL, "<ssid>", "SSID of AP");
    set_ap_args.password = arg_str1(NULL, NULL, "<passwd>", "Password of AP");
    set_ap_args.end = arg_end(2);

    const esp_console_cmd_t cmd = {
        .command = "set_ap",
        .help = "Set SSID and password of the SoftAP",
        .hint = NULL,
        .func = &set_ap,
        .argtable = &set_ap_args};
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}

/** Arguments used by 'set_ap_ip' function */
static struct
{
    struct arg_str *ap_ip_str;
    struct arg_end *end;
} set_ap_ip_arg;

/* 'set_ap_ip' command */
int set_ap_ip(int argc, char **argv)
{
    nvs_handle_t nvs;

    int nerrors = arg_parse(argc, argv, (void **)&set_ap_ip_arg);
    if (nerrors != 0)
    {
        arg_print_errors(stderr, set_ap_ip_arg.end, argv[0]);
        return ESP_FAIL;
    }

    ESP_ERROR_CHECK(nvs_open(PARAM_NAMESPACE, NVS_READWRITE, &nvs));
    ESP_ERROR_CHECK(nvs_set_str(nvs, "ap_ip", set_ap_ip_arg.ap_ip_str->sval[0]));
    ESP_ERROR_CHECK(nvs_commit(nvs));
    ESP_LOGI(TAG, "AP IP address %s stored.", set_ap_ip_arg.ap_ip_str->sval[0]);
    nvs_close(nvs);
    return ESP_OK;
}

static void register_set_ap_ip(void)
{
    set_ap_ip_arg.ap_ip_str = arg_str1(NULL, NULL, "<ip>", "IP");
    set_ap_ip_arg.end = arg_end(1);

    const esp_console_cmd_t cmd = {
        .command = "set_ap_ip",
        .help = "Set IP for the AP interface",
        .hint = NULL,
        .func = &set_ap_ip,
        .argtable = &set_ap_ip_arg};
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}

/** Arguments used by 'portmap' function */
static struct
{
    struct arg_str *add_del;
    struct arg_str *TCP_UDP;
    struct arg_int *ext_port;
    struct arg_str *int_ip;
    struct arg_int *int_port;
    struct arg_end *end;
} portmap_args;

/* 'portmap' command */
int portmap(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&portmap_args);
    if (nerrors != 0)
    {
        arg_print_errors(stderr, portmap_args.end, argv[0]);
        return ESP_FAIL;
    }

    bool add;
    if (strcmp((char *)portmap_args.add_del->sval[0], "add") == 0)
    {
        add = true;
    }
    else if (strcmp((char *)portmap_args.add_del->sval[0], "del") == 0)
    {
        add = false;
    }
    else
    {
        ESP_LOGW(TAG, "Must be 'add' or 'del");
        return ESP_FAIL;
    }

    uint8_t tcp_udp;
    if (strcmp((char *)portmap_args.TCP_UDP->sval[0], "TCP") == 0)
    {
        tcp_udp = PROTO_TCP;
    }
    else if (strcmp((char *)portmap_args.TCP_UDP->sval[0], "UDP") == 0)
    {
        tcp_udp = PROTO_UDP;
    }
    else
    {
        ESP_LOGW(TAG, "Must be 'TCP' or 'UDP'");
        return ESP_FAIL;
    }

    uint16_t ext_port = portmap_args.ext_port->ival[0];
    uint32_t int_ip = ipaddr_addr((char *)portmap_args.int_ip->sval[0]);
    uint16_t int_port = portmap_args.int_port->ival[0];

    if (add)
    {
        ESP_ERROR_CHECK(add_portmap(tcp_udp, ext_port, int_ip, int_port));
    }
    else
    {
        ESP_ERROR_CHECK(del_portmap(tcp_udp, ext_port, int_ip, int_port));
    }

    return ESP_OK;
}

static void register_portmap(void)
{
    portmap_args.add_del = arg_str1(NULL, NULL, "[add|del]", "add or delete portmapping");
    portmap_args.TCP_UDP = arg_str1(NULL, NULL, "[TCP|UDP]", "TCP or UDP port");
    portmap_args.ext_port = arg_int1(NULL, NULL, "<ext_portno>", "external port number");
    portmap_args.int_ip = arg_str1(NULL, NULL, "<int_ip>", "internal IP");
    portmap_args.int_port = arg_int1(NULL, NULL, "<int_portno>", "internal port number");
    portmap_args.end = arg_end(5);

    const esp_console_cmd_t cmd = {
        .command = "portmap",
        .help = "Add or delete a portmapping to the router",
        .hint = NULL,
        .func = &portmap,
        .argtable = &portmap_args};
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}

/* 'show' command */
static int show(int argc, char **argv)
{
    char *ssid = NULL;
    char *passwd = NULL;
    char *static_ip = NULL;
    char *subnet_mask = NULL;
    char *gateway_addr = NULL;
    char *ap_ssid = NULL;
    char *ap_passwd = NULL;

    get_config_param_str("ssid", &ssid);
    get_config_param_str("passwd", &passwd);
    get_config_param_str("static_ip", &static_ip);
    get_config_param_str("subnet_mask", &subnet_mask);
    get_config_param_str("gateway_addr", &gateway_addr);
    get_config_param_str("ap_ssid", &ap_ssid);
    get_config_param_str("ap_passwd", &ap_passwd);

    printf("STA SSID: %s Password: %s\n", ssid != NULL ? ssid : "<undef>",
           passwd != NULL ? passwd : "<undef>");
    printf("AP SSID: %s Password: %s\n", ap_ssid != NULL ? ap_ssid : "<undef>",
           ap_passwd != NULL ? ap_passwd : "<undef>");
    ip4_addr_t addr;
    addr.addr = my_ap_ip;
    printf("AP IP address: " IPSTR "\n", IP2STR(&addr));

    if (ssid != NULL)
        free(ssid);
    if (passwd != NULL)
        free(passwd);
    if (static_ip != NULL)
        free(static_ip);
    if (subnet_mask != NULL)
        free(subnet_mask);
    if (gateway_addr != NULL)
        free(gateway_addr);
    if (ap_ssid != NULL)
        free(ap_ssid);
    if (ap_passwd != NULL)
        free(ap_passwd);

    printf("Uplink AP %sconnected\n", ap_connect ? "" : "not ");
    if (ap_connect)
    {
        addr.addr = my_ip;
        printf("IP: " IPSTR "\n", IP2STR(&addr));
    }
    printf("%d Stations connected\n", getConnectCount());

    print_portmap_tab();

    return 0;
}

static void register_show(void)
{
    const esp_console_cmd_t cmd = {
        .command = "show",
        .help = "Get status and config of the router",
        .hint = NULL,
        .func = &show,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}

/** Arguments used by 'static_ip' function */
static struct
{
    struct arg_str *add_del;
    struct arg_str *ip_addr;
    struct arg_str *mac_addr;
    struct arg_end *end;
} static_ip_args;

/* 'static_ip' command */
int set_static_ip(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&static_ip_args);
    if (nerrors != 0)
    {
        arg_print_errors(stderr, static_ip_args.end, argv[0]);
        return ESP_FAIL;
    }

    bool add;
    if (strcmp((char *)static_ip_args.add_del->sval[0], "add") == 0)
    {
        add = true;
    }
    else if (strcmp((char *)static_ip_args.add_del->sval[0], "del") == 0)
    {
        add = false;
    }
    else
    {
        ESP_LOGW(TAG, "Must be 'add' or 'del");
        return ESP_FAIL;
    }

    char *ip_address = (char *)static_ip_args.ip_addr->sval[0];
    char *mac_address = (char *)static_ip_args.mac_addr->sval[0];

    if (add)
    {
        ESP_ERROR_CHECK(add_static_ip(ip_address, mac_address));
    }
    else
    {
        ESP_ERROR_CHECK(del_static_ip(ip_address, mac_address));
    }

    return ESP_OK;
}

static void register_static_ip(void)
{
    static_ip_args.add_del = arg_str1(NULL, NULL, "[add|del]", "add or delete static IP assignment");
    static_ip_args.ip_addr = arg_str1(NULL, NULL, "<ip_address>", "IP Address (ex. 192.168.4.2)");
    static_ip_args.mac_addr = arg_str1(NULL, NULL, "<mac_address>", "MAC Address (ex. AA:BB:CC:DD:EE:FF)");
    static_ip_args.end = arg_end(3);

    const esp_console_cmd_t cmd = {
        .command = "static_ip",
        .help = "Set/Unset a static IP assignment",
        .hint = NULL,
        .func = &set_static_ip,
        .argtable = &static_ip_args};
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}