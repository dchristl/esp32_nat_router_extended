#include "handler.h"
#include "timer.h"
#include "helper.h"

#include <sys/param.h>
#include <timer.h>

#include "nvs.h"
#include "cmd_nvs.h"
#include "router_globals.h"
#include "esp_wifi.h"

static const char *TAG = "ApplyHandler";

void setApByQuery(char *urlContent, nvs_handle_t nvs)
{
    size_t contentLength = 64;
    char param[contentLength];
    readUrlParameterIntoBuffer(urlContent, "ap_ssid", param, contentLength);
    ESP_ERROR_CHECK(nvs_set_str(nvs, "ap_ssid", param));
    readUrlParameterIntoBuffer(urlContent, "ap_password", param, contentLength);
    if (strlen(param) < 8)
    {
        nvs_erase_key(nvs, "ap_passwd");
    }
    else
    {
        ESP_ERROR_CHECK(nvs_set_str(nvs, "ap_passwd", param));
    }

    readUrlParameterIntoBuffer(urlContent, "ssid_hidden", param, contentLength);
    if (strcmp(param, "on") == 0)
    {
        ESP_LOGI(TAG, "AP-SSID should be hidden.");
        ESP_ERROR_CHECK(nvs_set_i32(nvs, "ssid_hidden", 1));
    }
    else
    {
        nvs_erase_key(nvs, "ssid_hidden");
    }
}

void setStaByQuery(char *urlContent, nvs_handle_t nvs)
{

    size_t contentLength = 64;
    char param[contentLength];
    readUrlParameterIntoBuffer(urlContent, "ssid", param, contentLength);
    ESP_ERROR_CHECK(nvs_set_str(nvs, "ssid", param));
    readUrlParameterIntoBuffer(urlContent, "password", param, contentLength);
    ESP_ERROR_CHECK(nvs_set_str(nvs, "passwd", param));
}
void setWpa2(char *urlContent, nvs_handle_t nvs)
{
    size_t contentLength = strlen(urlContent);
    char param[contentLength];
    readUrlParameterIntoBuffer(urlContent, "sta_identity", param, contentLength);
    if (strlen(param) > 0)
    {
        ESP_LOGI(TAG, "WPA2 Identity set to '%s'", param);
        ESP_ERROR_CHECK(nvs_set_str(nvs, "sta_identity", param));
    }
    else
    {
        ESP_LOGI(TAG, "WPA2 Identity will be deleted");
        nvs_erase_key(nvs, "sta_identity");
    }

    readUrlParameterIntoBuffer(urlContent, "sta_user", param, contentLength);

    if (strlen(param) > 0)
    {
        ESP_LOGI(TAG, "WPA2 user set to '%s'", param);
        ESP_ERROR_CHECK(nvs_set_str(nvs, "sta_user", param));
    }
    else
    {
        ESP_LOGI(TAG, "WPA2 user will be deleted");
        nvs_erase_key(nvs, "sta_user");
    }
    readUrlParameterIntoBuffer(urlContent, "cer", param, contentLength);

    if (strlen(param) > 0)
    {
        nvs_erase_key(nvs, "cer"); // do not double size in nvs
        ESP_LOGI(TAG, "Certificate with size %d set", strlen(param));
        ESP_ERROR_CHECK(nvs_set_blob(nvs, "cer", param, contentLength));
    }
    else
    {
        ESP_LOGI(TAG, "Certificate will be deleted");
        nvs_erase_key(nvs, "cer");
    }
}

void applyApStaConfig(char *buf)
{
    nvs_handle_t nvs;
    ESP_ERROR_CHECK(nvs_open(PARAM_NAMESPACE, NVS_READWRITE, &nvs));
    setApByQuery(buf, nvs);
    setStaByQuery(buf, nvs);
    setWpa2(buf, nvs);
    ESP_ERROR_CHECK(nvs_commit(nvs));
    nvs_close(nvs);
}

void eraseNvs()
{
    ESP_LOGW(TAG, "Erasing %s", PARAM_NAMESPACE);
    int argc = 2;
    char *argv[argc];
    argv[0] = "erase_namespace";
    argv[1] = PARAM_NAMESPACE;
    erase_ns(argc, argv);
}

void setDNSToDefault(nvs_handle_t *nvs)
{
    nvs_erase_key(*nvs, "custom_dns");
    ESP_LOGI(TAG, "DNS set to default (uplink network)");
}

void setMACToDefault(nvs_handle_t *nvs)
{
    nvs_erase_key(*nvs, "custom_mac");
    ESP_LOGI(TAG, "MAC set to default");
}

bool str2mac(const char *mac)
{
    uint8_t values[6] = {0};
    if (6 == sscanf(mac, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &values[0], &values[1], &values[2], &values[3], &values[4], &values[5]))
    {
        return true;
    }
    else
    {
        return false;
    }
}

char *getRedirectUrl(httpd_req_t *req)
{

    size_t buf_len = 16;
    char *host = malloc(buf_len);
    httpd_req_get_hdr_value_str(req, "Host", host, buf_len);
    ESP_LOGI(TAG, "Host of request is '%s'", host);
    char *str = malloc(strlen("http://") + buf_len);
    strcpy(str, "http://");
    if (strcmp(host, DEFAULT_AP_IP_CLASS_A) == 0 || strcmp(host, DEFAULT_AP_IP_CLASS_B) == 0 || strcmp(host, DEFAULT_AP_IP_CLASS_C) == 0)
    {
        strcat(str, getDefaultIPByNetmask());
    }
    else
    {
        strcat(str, host);
    }
    free(host);

    return str;
}

void applyAdvancedConfig(char *buf)
{
    ESP_LOGI(TAG, "Applying advanced config");
    nvs_handle_t nvs;
    nvs_open(PARAM_NAMESPACE, NVS_READWRITE, &nvs);

    size_t contentLength = 64;
    char param[contentLength];
    readUrlParameterIntoBuffer(buf, "keepalive", param, contentLength);

    if (strlen(param) > 0)
    {
        ESP_LOGI(TAG, "keep alive will be enabled");
        nvs_set_i32(nvs, "keep_alive", 1);
    }
    else
    {
        ESP_LOGI(TAG, "keep alive will be disabled");
        nvs_set_i32(nvs, "keep_alive", 0);
    }

    readUrlParameterIntoBuffer(buf, "ledenabled", param, contentLength);
    if (strlen(param) > 0)
    {
        ESP_LOGI(TAG, "ON Board LED will be enabled");
        nvs_set_i32(nvs, "led_disabled", 0);
    }
    else
    {
        ESP_LOGI(TAG, "ON Board LED will be disabled");
        nvs_set_i32(nvs, "led_disabled", 1);
    }

    readUrlParameterIntoBuffer(buf, "natenabled", param, contentLength);
    if (strlen(param) > 0)
    {
        ESP_LOGI(TAG, "NAT will be enabled");
        nvs_set_i32(nvs, "nat_disabled", 0);
    }
    else
    {
        ESP_LOGI(TAG, "NAT will be disabled");
        nvs_set_i32(nvs, "nat_disabled", 1);
    }

    readUrlParameterIntoBuffer(buf, "wsenabled", param, contentLength);
    if (strlen(param) == 0)
    {
        ESP_LOGI(TAG, "Webserver will be disabled");
        nvs_set_i32(nvs, "lock", 1);
    }

    readUrlParameterIntoBuffer(buf, "custommac", param, contentLength);
    if (strlen(param) > 0)
    {
        char macaddress[contentLength];
        readUrlParameterIntoBuffer(buf, "macaddress", macaddress, contentLength);
        if (strcmp("random", param) == 0)
        {
            ESP_LOGI(TAG, "MAC address set to random");
            nvs_set_str(nvs, "custom_mac", param);
        }
        else if (strlen(macaddress) > 0)
        {
            int success = str2mac(macaddress);
            if (success)
            {
                ESP_LOGI(TAG, "MAC address set to: %s", macaddress);
                nvs_set_str(nvs, "custom_mac", macaddress);
            }
            else
            {
                ESP_LOGI(TAG, "MAC address '%s' is invalid", macaddress);
                setMACToDefault(&nvs);
            }
        }
        else
        {
            setMACToDefault(&nvs);
        }
    }
    readUrlParameterIntoBuffer(buf, "dns", param, contentLength);
    if (strlen(param) > 0)
    {
        if (strcmp(param, "custom") == 0)
        {
            char customDnsParam[contentLength];
            readUrlParameterIntoBuffer(buf, "dnsip", customDnsParam, contentLength);
            if (strlen(customDnsParam) > 0)
            {
                uint32_t ipasInt = esp_ip4addr_aton(customDnsParam);
                if (ipasInt == UINT32_MAX || ipasInt == 0)
                {
                    ESP_LOGW(TAG, "Invalid custom DNS. Setting back to default!");
                    setDNSToDefault(&nvs);
                }
                else
                {
                    esp_ip4_addr_t *addr = malloc(16);
                    addr->addr = ipasInt;
                    esp_ip4addr_ntoa(addr, customDnsParam, 16);
                    ESP_LOGI(TAG, "DNS set to: %s", customDnsParam);
                    nvs_set_str(nvs, "custom_dns", customDnsParam);
                }
            }
            else
            {
                setDNSToDefault(&nvs);
            }
        }
        else
        {
            ESP_LOGI(TAG, "DNS set to: %s", param);
            nvs_set_str(nvs, "custom_dns", param);
        }
    }
    else
    {
        setDNSToDefault(&nvs);
    }
    readUrlParameterIntoBuffer(buf, "netmask", param, contentLength);
    if (strlen(param) > 0)
    {
        if (strcmp("classa", param) == 0)
        {
            ESP_LOGI(TAG, "Netmask set to Class A");
            nvs_set_str(nvs, "netmask", DEFAULT_NETMASK_CLASS_A);
        }
        else if (strcmp("classb", param) == 0)
        {
            ESP_LOGI(TAG, "Netmask set to Class B");
            nvs_set_str(nvs, "netmask", DEFAULT_NETMASK_CLASS_B);
        }
        else
        {
            ESP_LOGI(TAG, "Netmask set to Class C");
            nvs_set_str(nvs, "netmask", DEFAULT_NETMASK_CLASS_C);
        }
    }

    nvs_commit(nvs);
    nvs_close(nvs);
}

esp_err_t apply_get_handler(httpd_req_t *req)
{

    if (isLocked())
    {
        return redirectToLock(req);
    }
    extern const char apply_start[] asm("_binary_apply_html_start");
    extern const char apply_end[] asm("_binary_apply_html_end");
    ESP_LOGI(TAG, "Requesting apply page");
    closeHeader(req);

    char *redirectUrl = getRedirectUrl(req);
    char *apply_page = malloc(apply_end - apply_start + strlen(redirectUrl) - 2);

    ESP_LOGI(TAG, "Redirecting after apply to '%s'", redirectUrl);
    sprintf(apply_page, apply_start, redirectUrl);
    free(redirectUrl);

    return httpd_resp_send(req, apply_page, HTTPD_RESP_USE_STRLEN);
}
esp_err_t apply_post_handler(httpd_req_t *req)
{
    if (isLocked())
    {
        return redirectToLock(req);
    }
    httpd_req_to_sockfd(req);

    int remaining = req->content_len;
    int ret = 0;
    int bufferLength = req->content_len;
    ESP_LOGI(TAG, "Content length  => %d", req->content_len);
    char buf[1000]; // 1000 byte chunk
    char content[bufferLength];
    strcpy(content, ""); // Fill initial

    while (remaining > 0)
    {
        /* Read the data for the request */
        if ((ret = httpd_req_recv(req, buf, MIN(remaining, sizeof(buf)))) <= 0)
        {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT)
            {
                continue;
            }
            ESP_LOGE(TAG, "Timeout occured %d", ret);
            return ESP_FAIL;
        }
        buf[ret + 1] = '\0'; // add NUL terminator
        strcat(content, buf);
        remaining -= ret;
        ESP_LOGI(TAG, "%d bytes total received -> %d left", strlen(content), remaining);
    }
    char funcParam[9];

    ESP_LOGI(TAG, "getting content %s", content);

    readUrlParameterIntoBuffer(content, "func", funcParam, 9);

    ESP_LOGI(TAG, "Function => %s", funcParam);

    if (strcmp(funcParam, "config") == 0)
    {
        applyApStaConfig(content);
    }
    if (strcmp(funcParam, "erase") == 0)
    {
        eraseNvs();
    }
    if (strcmp(funcParam, "advanced") == 0)
    {
        applyAdvancedConfig(content);
    }
    restartByTimer();

    return apply_get_handler(req);
}