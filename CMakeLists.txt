cmake_minimum_required(VERSION 3.16.0)
add_definitions(
-DARDUINO_USB_MODE
-DARDUINO_USB_CDC_ON_BOOT
)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp32_nat_router)
