#include "handler.h"
#include "timer.h"

#include <sys/param.h>
#include <timer.h>

#include "nvs.h"
#include "cmd_nvs.h"
#include "router_globals.h"
#include "esp_wifi.h"
#include "esp_wifi_ap_get_sta_list.h"

static const char *TAG = "PortMapHandler";

esp_err_t portmap_get_handler(httpd_req_t *req)
{
    if (isLocked())
    {
        return redirectToLock(req);
    }

    httpd_req_to_sockfd(req);
    extern const char portmap_start[] asm("_binary_portmap_html_start");
    extern const char portmap_end[] asm("_binary_portmap_html_end");
    const size_t portmap_html_size = (portmap_end - portmap_start);
    char *result = "";
    int size = portmap_html_size + strlen(result);
    char *portmap_page = malloc(size - 2);
    sprintf(portmap_page, portmap_start, result);

    closeHeader(req);

    esp_err_t ret = httpd_resp_send(req, portmap_page, HTTPD_RESP_USE_STRLEN);
    free(portmap_page);
    ESP_LOGI(TAG, "Requesting portmap page");
    return ret;
}

esp_err_t portmap_post_handler(httpd_req_t *req)
{
    if (isLocked())
    {
        return redirectToLock(req);
    }

    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "/result");
    return httpd_resp_send(req, NULL, 0);
}
