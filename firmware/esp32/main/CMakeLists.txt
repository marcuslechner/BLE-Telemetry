idf_component_register(SRCS "main.c" "esp_link.c" "ble_battery.c"
                       REQUIRES bt esp_driver_uart nvs_flash
                       INCLUDE_DIRS ".")
