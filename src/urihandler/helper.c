#include "helper.h"
#include <ctype.h>

static const char *TAG = "urihelper";

void preprocess_string(char *str)
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

void readUrlParameterIntoBuffer(char *parameterString, char *parameter, char *buffer, size_t paramLength)
{
    if (httpd_query_key_value(parameterString, parameter, buffer, paramLength) == ESP_OK)
    {
        preprocess_string(buffer);
        ESP_LOGI(TAG, "Found '%s' parameter => %s", parameter, buffer);
    }
    else
    {
        ESP_LOGI(TAG, "Parameter '%s' not found", parameter);
        buffer[0] = '\0';
    }
}

esp_err_t fill_post_buffer(httpd_req_t *req, char *buf, size_t len)
{
    int ret, remaining = len;

    while (remaining > 0)
    {
        /* Read the data for the request */
        if ((ret = httpd_req_recv(req, buf, MIN(remaining, len))) <= 0)
        {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT)
            {
                continue;
            }
            ESP_LOGE(TAG, "Timeout occurred");
            return ESP_FAIL;
        }

        remaining -= ret;
    }

    return ESP_OK;
}

bool is_valid_subnet_mask(char *subnet_mask)
{
    char *token;
    int octet;
    int count = 0;

    // ip_addr_t ip_addr;
    // if (ipaddr_aton(subnet_mask, &ip_addr))
    // {
    //     // IP-Adresse wurde erfolgreich umgewandelt

    //     // Schleife, um jedes Bit auszugeben
    //     char buf[65];
    //     u_int32_t num = ip4_addr_get_u32(ip_2_ip4(&ip_addr));
    //     // Ausgabe in binärer Form mit dem ESP-IDF-Logframework
    //     ESP_LOGW(TAG, "Binäre Darstellung von %lu: %s", num, utoa(num, buf, 2));
    // }
    // else
    // {
    //     return false;
    // }

    // Copy for calculation
    char mask_copy[strlen(subnet_mask) + 1];
    strcpy(mask_copy, subnet_mask);

    // split by dots
    token = strtok(mask_copy, ".");

    while (token != NULL)
    {
        // Convert to int
        octet = atoi(token);
        // Valid value between 0 and 255
        if (octet < 0 || octet > 255)
        {
            ESP_LOGE(TAG, "%s is not a valid subnet mask. Octet %d out of range.", subnet_mask, octet);
            return false;
        }

        // Count octetts
        count++;

        token = strtok(NULL, ".");
    }

    // Exactly 4 octetts
    if (count != 4)
    {
        ESP_LOGE(TAG, "%s is not a valid subnet mask. Not exactly 4 octets.", subnet_mask);
        return false;
    }

    // Check bits. Every bit other the last 1 must be 0
    unsigned int mask_value = 0;
    strcpy(mask_copy, subnet_mask);
    token = strtok(mask_copy, ".");
    while (token != NULL)
    {
        octet = atoi(token);
        mask_value = (mask_value << 8) | octet;
        token = strtok(NULL, ".");
    }
    unsigned int inverted_mask = ~mask_value;
    if ((inverted_mask & (inverted_mask + 1)) != 0)
    {
        ESP_LOGE(TAG, "%s is not a valid subnet mask. The bits after the last 1 have to be zero.", subnet_mask);
        return false;
    }

    return true;
}