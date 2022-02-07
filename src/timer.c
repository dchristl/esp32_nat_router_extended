#include "timer.h"
#include <esp_wifi.h>
#include <esp_log.h>
#include "esp_http_client.h"

static const char *TAG = "Timer";

#define REFRESH_TIMER_PERIOD 5 * 600000

esp_timer_handle_t restart_timer, refresh_timer;

static void restart_timer_callback(void *arg)
{
    ESP_LOGI(TAG, "Restarting now...");
    esp_restart();
}
static void refresh_timer_callback(void *arg)
{
    ESP_LOGI(TAG, "Starting ws call");
    esp_http_client_config_t config = {
        .url = "https://www.startpage.com/",
        .method = HTTP_METHOD_HEAD,
        .disable_auto_redirect = true};
    esp_http_client_handle_t client = esp_http_client_init(&config);

    // GET
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK)
    {
        ESP_LOGI(TAG, "HTTP GET Status = %d, content_length = %d ", esp_http_client_get_status_code(client),
                 esp_http_client_get_content_length(client));
    }
    else
    {
        ESP_LOGE(TAG, "HTTP GET request failed: %s", esp_err_to_name(err));
    }
    esp_http_client_cleanup(client);
}

esp_timer_create_args_t restart_timer_args = {
    .callback = &restart_timer_callback,
    /* argument specified here will be passed to timer callback function */
    .arg = (void *)0,
    .name = "restart_timer"};

esp_timer_create_args_t refresh_timer_args = {
    .callback = &refresh_timer_callback,
    .arg = (void *)0,
    .name = "refresh_timer"};

void initializeRestartTimer()
{
    esp_timer_create(&restart_timer_args, &restart_timer);
}
void initializeKeepAliveTimer()
{
    esp_timer_create(&refresh_timer_args, &refresh_timer);
    esp_timer_start_periodic(refresh_timer, REFRESH_TIMER_PERIOD);
}

void restartByTimer()
{
    esp_timer_start_once(restart_timer, 500000);
}