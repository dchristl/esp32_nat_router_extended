#pragma once

// Spawn a low-priority task that watches for semantic lockups (AP down, STA
// stuck, heap exhausted, DNS server dead) and calls esp_restart() after
// LIVENESS_FAIL_THRESHOLD consecutive failed ticks. Intended for the case
// where every task is still nominally alive (so the IDF Task WDT never fires)
// but the router has stopped routing. Call once near the end of app_main.
void start_liveness_watchdog(void);

// Reset the STA-reconnect backoff (call from STA_GOT_IP).
void reconnect_backoff_reset(void);

// Schedule esp_wifi_connect() with exponential backoff (call from
// STA_DISCONNECTED). Safe to call from a WiFi event handler.
void reconnect_backoff_schedule(void);
