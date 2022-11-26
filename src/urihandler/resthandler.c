#include "handler.h"
#include "router_globals.h"

static const char *TAG = "RestHandler";

const char *JSON_TEMPLATE = "{\"clients\": %d,\"strength\": %s,\"text\": \"%s\",\"symbol\": \"%s\"}";

esp_err_t rest_handler(httpd_req_t *req)
{
    if (isLocked())
    {
        return httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, NULL);
    }
    httpd_resp_set_type(req, "application/json");

    char *db = NULL;
    char *symbol = NULL;
    char *textColor = NULL;
    fillInfoData(&db, &symbol, &textColor);

    size_t size = strlen(JSON_TEMPLATE) + 5 + strlen(db) + strlen(textColor) + strlen(symbol);
    char *json = malloc(size);
    sprintf(json, JSON_TEMPLATE, getConnectCount(), db, textColor, symbol);
    esp_err_t ret = httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
    ESP_LOGD(TAG, "JSON-Response: %s", json);
    free(json);
    free(db);
    return ret;
}