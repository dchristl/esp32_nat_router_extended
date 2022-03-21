#include "handler.h"
#include "timer.h"

#include <sys/param.h>
#include <timer.h>

#include "nvs.h"
#include "cmd_nvs.h"
#include "router_globals.h"
#include "esp_wifi.h"

static const char *TAG = "ApplyHandler";

void fillParamArray(char *buf, char *argv[], char *ssidKey, char *passKey)
{
    char ssidParam[32];
    char passParam[64];

    if (httpd_query_key_value(buf, ssidKey, ssidParam, 32) == ESP_OK)
    {
        preprocess_string(ssidParam);
        ESP_LOGI(TAG, "Found URL query parameter => %s=%s", ssidKey, ssidParam);

        httpd_query_key_value(buf, passKey, passParam, 64);
        preprocess_string(passParam);
        ESP_LOGI(TAG, "Found URL query parameter => %s=%s", passKey, passParam);
        argv[1] = (char *)malloc(sizeof(char) * (strlen(ssidParam) + 1));
        argv[2] = (char *)malloc(sizeof(char) * (strlen(passParam) + 1));
        strcpy(argv[1], ssidParam);
        strcpy(argv[2], passParam);
    }
}

void setApByQuery(char *buf, nvs_handle_t nvs)
{
    int argc = 3;
    char *argv[argc];
    argv[0] = "set_ap";
    fillParamArray(buf, argv, "ap_ssid", "ap_password");
    ESP_ERROR_CHECK(nvs_set_str(nvs, "ap_ssid", argv[1]));
    ESP_ERROR_CHECK(nvs_set_str(nvs, "ap_passwd", argv[2]));
}

void setStaByQuery(char *buf, nvs_handle_t nvs)
{
    int argc = 3;
    char *argv[argc];
    argv[0] = "set_sta";
    fillParamArray(buf, argv, "ssid", "password");

    ESP_ERROR_CHECK(nvs_set_str(nvs, "ssid", argv[1]));
    ESP_ERROR_CHECK(nvs_set_str(nvs, "passwd", argv[2]));
}

void applyApStaConfig(char *buf)
{

    char *postCopy;
    postCopy = malloc(sizeof(char) * (strlen(buf) + 1));
    strcpy(postCopy, buf);
    nvs_handle_t nvs;
    ESP_ERROR_CHECK(nvs_open(PARAM_NAMESPACE, NVS_READWRITE, &nvs));
    setApByQuery(postCopy, nvs);
    setStaByQuery(postCopy, nvs);
    ESP_ERROR_CHECK(nvs_commit(nvs));
    nvs_close(nvs);
    free(postCopy);
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

void applyAdvancedConfig(char *buf)
{
    ESP_LOGI(TAG, "Applying advanced config");
    nvs_handle_t nvs;
    nvs_open(PARAM_NAMESPACE, NVS_READWRITE, &nvs);

    char keepAliveParam[strlen(buf)], ledParam[strlen(buf)], lockParam[strlen(buf)], dnsParam[strlen(buf)], customDnsParam[strlen(buf)], macaddress[strlen(buf)];
    if (httpd_query_key_value(buf, "keepalive", keepAliveParam, sizeof(keepAliveParam)) == ESP_OK)
    {
        preprocess_string(keepAliveParam);
        ESP_LOGI(TAG, "keep alive will be enabled");
        nvs_set_i32(nvs, "keep_alive", 1);
    }
    else
    {
        ESP_LOGI(TAG, "keep alive will be disabled");
        nvs_set_i32(nvs, "keep_alive", 0);
    }

    if (httpd_query_key_value(buf, "ledenabled", ledParam, sizeof(ledParam)) == ESP_OK)
    {
        preprocess_string(ledParam);
        ESP_LOGI(TAG, "ON Board LED will be enabled");
        nvs_set_i32(nvs, "led_disabled", 0);
    }
    else
    {
        ESP_LOGI(TAG, "ON Board LED will be disabled");
        nvs_set_i32(nvs, "led_disabled", 1);
    }
    if (httpd_query_key_value(buf, "wsenabled", lockParam, sizeof(lockParam)) != ESP_OK)
    {
        ESP_LOGI(TAG, "Webserver will be disabled");
        nvs_set_i32(nvs, "lock", 1);
    }

    if (httpd_query_key_value(buf, "macaddress", macaddress, sizeof(macaddress)) == ESP_OK)
    {
        preprocess_string(macaddress);
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

    if (httpd_query_key_value(buf, "dns", dnsParam, sizeof(dnsParam)) == ESP_OK)
    {
        preprocess_string(dnsParam);
        if (strlen(dnsParam) == 0)
        {
            setDNSToDefault(&nvs);
        }
        else if (strcmp(dnsParam, "custom") == 0)
        {
            if (httpd_query_key_value(buf, "dnsip", customDnsParam, sizeof(customDnsParam)) == ESP_OK)
            {
                uint32_t ipasInt = esp_ip4addr_aton(customDnsParam);
                if (ipasInt == UINT32_MAX || ipasInt == 0)
                {
                    ESP_LOGW(TAG, "Invalid custom DNS. Setting back to default!");
                    setDNSToDefault(&nvs);
                }
                else
                {
                    esp_ip4_addr_t *addr = malloc(IP4ADDR_STRLEN_MAX);
                    addr->addr = ipasInt;
                    esp_ip4addr_ntoa(addr, customDnsParam, IP4ADDR_STRLEN_MAX);
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
            ESP_LOGI(TAG, "DNS set to: %s", dnsParam);
            nvs_set_str(nvs, "custom_dns", dnsParam);
        }
    }
    else
    {
        setDNSToDefault(&nvs);
    }

    nvs_commit(nvs);
    nvs_close(nvs);
}

esp_err_t apply_get_handler(httpd_req_t *req)
{

    if (isLocked())
    {
        return unlock_handler(req);
    }
    extern const char apply_start[] asm("_binary_apply_html_start");
    extern const char apply_end[] asm("_binary_apply_html_end");
    const size_t apply_html_size = (apply_end - apply_start);
    ESP_LOGI(TAG, "Requesting apply page");
    closeHeader(req);
    return httpd_resp_send(req, apply_start, apply_html_size);
}
esp_err_t apply_post_handler(httpd_req_t *req)
{
    if (isLocked())
    {
        return unlock_handler(req);
    }
    httpd_req_to_sockfd(req);

    int ret, remaining = req->content_len;
    char buf[req->content_len];

    while (remaining > 0)
    {
        /* Read the data for the request */
        if ((ret = httpd_req_recv(req, buf, MIN(remaining, sizeof(buf)))) <= 0)
        {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT)
            {
                continue;
            }
            ESP_LOGE(TAG, "Timeout occured");
            return ESP_FAIL;
        }

        remaining -= ret;
        ESP_LOGI(TAG, "Found parameter query => %s", buf);
        char funcParam[req->content_len];
        if (httpd_query_key_value(buf, "func", funcParam, sizeof(funcParam)) == ESP_OK)
        {
            ESP_LOGI(TAG, "Found function parameter => %s", funcParam);
            preprocess_string(funcParam);

            if (strcmp(funcParam, "config") == 0)
            {
                applyApStaConfig(buf);
            }
            if (strcmp(funcParam, "erase") == 0)
            {
                eraseNvs();
            }
            if (strcmp(funcParam, "advanced") == 0)
            {
                applyAdvancedConfig(buf);
            }
            restartByTimer(); 
        }
    }
    return apply_get_handler(req);
}