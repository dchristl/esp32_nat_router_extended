# This file was automatically generated for projects
# without default 'CMakeLists.txt' file.

FILE(GLOB_RECURSE app_sources ${CMAKE_SOURCE_DIR}/src/*.*)

idf_component_register(SRCS ${app_sources} "esp32_nat_router.c"
                            "http_server.c" "helper.c" INCLUDE_DIRS ".")

target_add_binary_data(${COMPONENT_TARGET} "pages/favicon.ico" TEXT)
target_add_binary_data(${COMPONENT_TARGET} "pages/styles.css" TEXT)
target_add_binary_data(${COMPONENT_TARGET} "pages/config.html" TEXT)
target_add_binary_data(${COMPONENT_TARGET} "pages/scan.html" TEXT)
target_add_binary_data(${COMPONENT_TARGET} "pages/apply.html" TEXT)