#include <string.h>
#include "freertos/FreeRTOS.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "router_globals.h"

void fillNodes();
char *findTextColorForSSID(int8_t rssi);