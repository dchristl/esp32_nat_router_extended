
#include "handler.h"
#include "scan.h"

#include "router_globals.h"

static const char *TAG = "ScanHandler";

static char BALLOT_BOX[] = "&#9744;";
static char BALLOT_BOX_WITH_CHECK[] = "&#9745;";

void fillInfoData(char **clients, char **db, char **symbol, char **textColor)
{

    *clients = "0";
    *db = malloc(5);
    sprintf(*db, "%d", 0);
    *symbol = BALLOT_BOX;
    *textColor = "info";
    wifi_ap_record_t apinfo;
    memset(&apinfo, 0, sizeof(apinfo));
    if (esp_wifi_sta_get_ap_info(&apinfo) == ESP_OK)
    {
        *db = malloc(5);
        sprintf(*db, "%d", apinfo.rssi);
        *symbol = BALLOT_BOX_WITH_CHECK;
        *textColor = findTextColorForSSID(apinfo.rssi);
        ESP_LOGI(TAG, "RSSI: %d", apinfo.rssi);
        // ESP_LOGI(TAG, "Channel: %d", apinfo.primary);
        ESP_LOGI(TAG, "SSID: %s", apinfo.ssid);
    };
    *clients = malloc(6);
    sprintf(*clients, "%i", connect_count);
}

esp_err_t scan_download_get_handler(httpd_req_t *req)
{

    if (isLocked())
    {
        return unlock_handler(req);
    }

    httpd_req_to_sockfd(req);

    extern const char scan_start[] asm("_binary_scan_html_start");
    extern const char scan_end[] asm("_binary_scan_html_end");
    const size_t scan_html_size = (scan_end - scan_start);

    closeHeader(req);

    ESP_LOGI(TAG, "Requesting scan page");

    esp_err_t ret = httpd_resp_send(req, scan_start, scan_html_size - 2);
    fillNodes();
    return ret;
}
