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