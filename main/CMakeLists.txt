idf_component_register(SRCS "esp32_nat_router.c"
                            "http_server.c"
                    INCLUDE_DIRS "." EMBED_TXTFILES pages/favicon.ico pages/scan.html pages/styles.css pages/config.html)
