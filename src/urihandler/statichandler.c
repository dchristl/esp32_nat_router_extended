#include "handler.h"

static const char *TAG_HANDLER = "LockHandler";

void closeHeader(httpd_req_t *req)
{
    httpd_resp_set_hdr(req, "Connection", "close");
}

esp_err_t download(httpd_req_t *req, const char *fileStart, const size_t fileSize)
{
    httpd_resp_set_hdr(req, "Cache-Control", "max-age=31536000");
    closeHeader(req);
    return httpd_resp_send(req, fileStart, fileSize);
}

esp_err_t styles_download_get_handler(httpd_req_t *req)
{
    extern const unsigned char styles_start[] asm("_binary_styles_1_css_start");
    extern const unsigned char styles_end[] asm("_binary_styles_1_css_end");
    const size_t styles_size = (styles_end - styles_start);
    httpd_resp_set_type(req, "text/css");
    ESP_LOGI(TAG_HANDLER, "Requesting style.css");
    return download(req, (const char *)styles_start, styles_size);
}

esp_err_t bootstrap_get_handler(httpd_req_t *req)
{
    extern const unsigned char bootstrap_js_start[] asm("_binary_bootstrap_js_start");
    extern const unsigned char bootstrap_js_end[] asm("_binary_bootstrap_js_end");
    const size_t bootstrap_js_size = (bootstrap_js_end - bootstrap_js_start);
    httpd_resp_set_type(req, "text/javascript");
    ESP_LOGI(TAG_HANDLER, "Requesting bootstrap");
    return download(req, (const char *)bootstrap_js_start, bootstrap_js_size);
}

esp_err_t jquery_get_handler(httpd_req_t *req)
{
    extern const unsigned char jquery_js_start[] asm("_binary_jquery_js_start");
    extern const unsigned char jquery_js_end[] asm("_binary_jquery_js_end");
    const size_t jquery_js_size = (jquery_js_end - jquery_js_start);
    httpd_resp_set_type(req, "text/javascript");
    ESP_LOGI(TAG_HANDLER, "Requesting jquery");
    return download(req, (const char *)jquery_js_start, jquery_js_size);
}

// Handler to download a "favicon.ico" file kept on the server
esp_err_t favicon_get_handler(httpd_req_t *req)
{
    extern const unsigned char favicon_ico_start[] asm("_binary_favicon_ico_start");
    extern const unsigned char favicon_ico_end[] asm("_binary_favicon_ico_end");
    const size_t favicon_ico_size = (favicon_ico_end - favicon_ico_start);
    httpd_resp_set_type(req, "image/x-icon");
    ESP_LOGI(TAG_HANDLER, "Requesting favicon");
    return download(req, (const char *)favicon_ico_start, favicon_ico_size);
}

esp_err_t http_404_error_handler(httpd_req_t *req, httpd_err_code_t err)
{
    httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Page not found");
    return ESP_FAIL;
}


esp_err_t reset_get_handler(httpd_req_t *req)
{
    httpd_req_to_sockfd(req);
    extern const char reset_start[] asm("_binary_reset_html_start");
    extern const char reset_end[] asm("_binary_reset_html_end");
    const size_t reset_html_size = (reset_end - reset_start);

    closeHeader(req);

    esp_err_t ret = httpd_resp_send(req, reset_start, reset_html_size);
    return ret;
}
