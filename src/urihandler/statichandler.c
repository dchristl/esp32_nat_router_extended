#include "handler.h"

static const char *TAG_HANDLER = "LockHandler";

void closeHeader(httpd_req_t *req)
{
    httpd_resp_set_hdr(req, "Connection", "close");
}

esp_err_t download(httpd_req_t *req, const char *fileStart)
{
    httpd_resp_set_hdr(req, "Cache-Control", "max-age=31536000");
    closeHeader(req);
    return httpd_resp_send(req, fileStart, HTTPD_RESP_USE_STRLEN);
}

esp_err_t styles_download_get_handler(httpd_req_t *req)
{
    extern const unsigned char styles_start[] asm("_binary_styles_9ee3c4491d35b3c1d830fa9da31c7861_css_start");
    httpd_resp_set_type(req, "text/css");
    ESP_LOGI(TAG_HANDLER, "Requesting style.css");
    return download(req, (const char *)styles_start);
}

esp_err_t bootstrap_get_handler(httpd_req_t *req)
{
    extern const unsigned char bootstrap_js_start[] asm("_binary_bootstrap_js_start");
    httpd_resp_set_type(req, "text/javascript");
    ESP_LOGI(TAG_HANDLER, "Requesting bootstrap");
    return download(req, (const char *)bootstrap_js_start);
}

esp_err_t jquery_get_handler(httpd_req_t *req)
{
    extern const unsigned char jquery_js_start[] asm("_binary_jquery_js_start");
    httpd_resp_set_type(req, "text/javascript");
    ESP_LOGI(TAG_HANDLER, "Requesting jquery");
    return download(req, (const char *)jquery_js_start);
}

// Handler to download a "favicon.ico" file kept on the server
esp_err_t favicon_get_handler(httpd_req_t *req)
{
    extern const char favicon_ico_start[] asm("_binary_favicon_ico_start");
    extern const char favicon_ico_end[] asm("_binary_favicon_ico_end");
    const size_t favicon_ico_size = (favicon_ico_end - favicon_ico_start);
    httpd_resp_set_type(req, "image/x-icon");
    ESP_LOGI(TAG_HANDLER, "Requesting favicon");
    closeHeader(req);
    return httpd_resp_send(req, favicon_ico_start, favicon_ico_size);
}

esp_err_t redirectToRoot(httpd_req_t *req)
{
    httpd_resp_set_type(req, HTTPD_TYPE_TEXT);
    httpd_resp_set_status(req, "301 Moved Permanently");
    char str[strlen("http://") + strlen(getDefaultIPByNetmask())];
    strcpy(str, "http://");
    strcat(str, getDefaultIPByNetmask());
    httpd_resp_set_hdr(req, "Location", str);
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

esp_err_t http_404_error_handler(httpd_req_t *req, httpd_err_code_t err)
{
    if (isDnsStarted())
    {
        return redirectToRoot(req);
    }
    else
    {
        return httpd_resp_send_404(req);
    }
}

esp_err_t reset_get_handler(httpd_req_t *req)
{
    httpd_req_to_sockfd(req);
    extern const char reset_start[] asm("_binary_reset_html_start");

    closeHeader(req);

    esp_err_t ret = httpd_resp_send(req, reset_start, HTTPD_RESP_USE_STRLEN);
    return ret;
}
