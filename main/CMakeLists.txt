idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES app_wifi app_web app_storage app_state bt_elm327 obd_service nvs_flash esp_timer
)
