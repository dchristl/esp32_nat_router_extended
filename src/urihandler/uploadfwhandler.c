// Custom-firmware upload via POST /uploadfw.
//
// Body is the raw .bin (no multipart). The frontend in ota.html submits the
// File object directly via fetch(). We stream chunks straight into
// esp_ota_write() so memory use is bounded regardless of image size, set the
// inactive partition as the boot target, and trigger a restart.

#include <esp_log.h>
#include <esp_ota_ops.h>
#include <esp_http_server.h>
#include <esp_partition.h>
#include <string.h>

#include "handler.h"
#include "timer.h"

static const char *TAG = "UploadFW";

#define UPLOAD_CHUNK_SIZE 4096

static esp_err_t reply_error(httpd_req_t *req, int code, const char *status, const char *msg)
{
    ESP_LOGE(TAG, "%s: %s", status, msg);
    httpd_resp_set_status(req, status);
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_sendstr(req, msg);
    return code;
}

esp_err_t uploadfw_post_handler(httpd_req_t *req)
{
    if (isLocked())
    {
        return redirectToLock(req);
    }

    const esp_partition_t *part = esp_ota_get_next_update_partition(NULL);
    if (!part)
    {
        return reply_error(req, ESP_OK, "500 Internal Server Error", "no OTA partition");
    }
    ESP_LOGI(TAG, "writing to partition %s (size %u)", part->label, (unsigned)part->size);

    int total = req->content_len;
    if (total <= 0)
    {
        return reply_error(req, ESP_OK, "411 Length Required", "Content-Length required");
    }
    if ((size_t)total > part->size)
    {
        return reply_error(req, ESP_OK, "413 Payload Too Large", "firmware larger than OTA partition");
    }

    esp_ota_handle_t handle = 0;
    esp_err_t err = esp_ota_begin(part, total, &handle);
    if (err != ESP_OK)
    {
        return reply_error(req, ESP_OK, "500 Internal Server Error", esp_err_to_name(err));
    }

    char *buf = malloc(UPLOAD_CHUNK_SIZE);
    if (!buf)
    {
        esp_ota_abort(handle);
        return reply_error(req, ESP_OK, "500 Internal Server Error", "no memory");
    }

    int received = 0;
    while (received < total)
    {
        int want = total - received;
        if (want > UPLOAD_CHUNK_SIZE)
            want = UPLOAD_CHUNK_SIZE;
        int n = httpd_req_recv(req, buf, want);
        if (n <= 0)
        {
            if (n == HTTPD_SOCK_ERR_TIMEOUT)
                continue;
            ESP_LOGE(TAG, "recv failed at %d/%d (n=%d)", received, total, n);
            free(buf);
            esp_ota_abort(handle);
            return reply_error(req, ESP_OK, "400 Bad Request", "upload truncated");
        }
        err = esp_ota_write(handle, buf, n);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "ota_write failed at %d: %s", received, esp_err_to_name(err));
            free(buf);
            esp_ota_abort(handle);
            return reply_error(req, ESP_OK, "500 Internal Server Error", esp_err_to_name(err));
        }
        received += n;
    }
    free(buf);

    err = esp_ota_end(handle);
    if (err != ESP_OK)
    {
        // ESP_ERR_OTA_VALIDATE_FAILED commonly means a bad image signature/header.
        return reply_error(req, ESP_OK, "400 Bad Request", esp_err_to_name(err));
    }
    err = esp_ota_set_boot_partition(part);
    if (err != ESP_OK)
    {
        return reply_error(req, ESP_OK, "500 Internal Server Error", esp_err_to_name(err));
    }

    ESP_LOGI(TAG, "upload OK (%d bytes), rebooting", received);
    httpd_resp_set_status(req, "200 OK");
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_sendstr(req, "OK; restarting");

    restartByTimerinS(2);
    return ESP_OK;
}
