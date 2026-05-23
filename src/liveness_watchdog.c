#include "liveness_watchdog.h"

#include <string.h>

#include "esp_log.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Globals defined in esp32_nat_router.c.
extern esp_netif_t *wifiAP;
extern esp_netif_t *wifiSTA;
extern bool ap_connect;
extern char *ssid;

// DNS task handle from dnserver.c (set non-NULL while the task is alive,
// cleared by stop_dns_server()).
extern TaskHandle_t task;

static const char *TAG = "Liveness";

#define LIVENESS_TICK_MS 10000
#define LIVENESS_FAIL_THRESHOLD 6     // 6 * 10s = ~60s grace
#define LIVENESS_HEAP_FLOOR_BYTES 16384

// STA outages can be legitimate (upstream AP is genuinely down for hours).
// Rebooting on STA-only failure takes the soft-AP down too, which removes the
// user's ability to reach the web UI to diagnose. So we only reboot for STA
// after a long grace, treating a multi-hour stuck STA as a likely radio wedge
// worth clearing. Tune to taste.
#define LIVENESS_STA_NO_IP_GRACE_TICKS 360 // 60 min

// ---- STA reconnect backoff ---------------------------------------------------

static esp_timer_handle_t reconnect_timer;
static int reconnect_attempts;

static const int RECONNECT_BACKOFF_S[] = {1, 2, 4, 8, 16, 30};
#define RECONNECT_BACKOFF_LEN (sizeof(RECONNECT_BACKOFF_S) / sizeof(RECONNECT_BACKOFF_S[0]))

static void reconnect_timer_cb(void *arg)
{
    esp_err_t err = esp_wifi_connect();
    if (err != ESP_OK)
    {
        ESP_LOGW(TAG, "esp_wifi_connect retry: %s", esp_err_to_name(err));
    }
}

static void reconnect_backoff_init(void)
{
    if (reconnect_timer)
        return;
    const esp_timer_create_args_t args = {
        .callback = &reconnect_timer_cb,
        .arg = NULL,
        .name = "wifi_reconnect"};
    esp_timer_create(&args, &reconnect_timer);
}

void reconnect_backoff_reset(void)
{
    reconnect_attempts = 0;
    if (reconnect_timer)
        esp_timer_stop(reconnect_timer);
}

void reconnect_backoff_schedule(void)
{
    reconnect_backoff_init();
    int idx = reconnect_attempts;
    if (idx >= (int)RECONNECT_BACKOFF_LEN)
        idx = RECONNECT_BACKOFF_LEN - 1;
    int delay_s = RECONNECT_BACKOFF_S[idx];
    reconnect_attempts++;
    esp_timer_stop(reconnect_timer); // ignore err if not running
    esp_timer_start_once(reconnect_timer, (uint64_t)delay_s * 1000000ULL);
    ESP_LOGI(TAG, "wifi reconnect in %ds (attempt %d)", delay_s, reconnect_attempts);
}

// ---- Liveness checks ---------------------------------------------------------

// Counts ticks since boot during which the STA is configured but never got an
// IP. Lets us survive transient upstream outages without rebooting, while
// still catching a permanent wedge.
static int sta_down_ticks;

static bool check_ap_up(void)
{
    return wifiAP && esp_netif_is_netif_up(wifiAP);
}

static bool check_sta_ok(void)
{
    if (!ssid || strlen(ssid) == 0)
        return true; // STA not configured; not our problem
    if (ap_connect)
    {
        sta_down_ticks = 0;
        return true;
    }
    sta_down_ticks++;
    if (sta_down_ticks > LIVENESS_STA_NO_IP_GRACE_TICKS)
    {
        ESP_LOGE(TAG, "STA has had no IP for %d ticks (%ds)",
                 sta_down_ticks, sta_down_ticks * (LIVENESS_TICK_MS / 1000));
        return false;
    }
    return true;
}

static bool check_heap(void)
{
    size_t free_heap = esp_get_free_heap_size();
    if (free_heap < LIVENESS_HEAP_FLOOR_BYTES)
    {
        ESP_LOGE(TAG, "free heap %u below floor %u", (unsigned)free_heap, LIVENESS_HEAP_FLOOR_BYTES);
        return false;
    }
    return true;
}

static bool check_dns_task(void)
{
    // task==NULL while DNS is intentionally stopped (e.g. after STA got IP).
    // Only complain if the handle is set but the task is gone.
    if (task == NULL)
        return true;
    eTaskState st = eTaskGetState(task);
    if (st == eDeleted || st == eInvalid)
    {
        ESP_LOGE(TAG, "DNS task handle non-NULL but state=%d", (int)st);
        return false;
    }
    return true;
}

static void liveness_task(void *arg)
{
    int consecutive_fails = 0;
    while (true)
    {
        vTaskDelay(pdMS_TO_TICKS(LIVENESS_TICK_MS));

        bool ok = true;
        if (!check_ap_up())
        {
            ESP_LOGE(TAG, "AP netif down");
            ok = false;
        }
        if (!check_sta_ok())
            ok = false;
        if (!check_heap())
            ok = false;
        if (!check_dns_task())
            ok = false;

        if (ok)
        {
            consecutive_fails = 0;
            continue;
        }
        consecutive_fails++;
        ESP_LOGW(TAG, "liveness fail %d/%d", consecutive_fails, LIVENESS_FAIL_THRESHOLD);
        if (consecutive_fails >= LIVENESS_FAIL_THRESHOLD)
        {
            ESP_LOGE(TAG, "liveness threshold reached, restarting");
            // Give UART time to flush.
            vTaskDelay(pdMS_TO_TICKS(200));
            esp_restart();
        }
    }
}

void start_liveness_watchdog(void)
{
    reconnect_backoff_init();
    // 3 KB stack, prio 1 (below tcpip_thread, above idle). Not subscribed to
    // TWDT — its job is to outlive other tasks' hangs.
    BaseType_t r = xTaskCreate(liveness_task, "liveness", 3072, NULL, 1, NULL);
    if (r != pdPASS)
        ESP_LOGE(TAG, "xTaskCreate failed: %d", (int)r);
}
