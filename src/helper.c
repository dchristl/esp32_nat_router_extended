#include "helper.h"

esp_err_t downloadStatic(httpd_req_t *req, const char *fileStart, const size_t fileSize)
{
  httpd_resp_set_hdr(req, "Cache-Control", "max-age=31536000");

  const char *field = "Connection";
  const char *value = "close";

  httpd_resp_set_hdr(req, field, value);

  return httpd_resp_send(req, fileStart, fileSize);
}