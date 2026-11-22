idf_component_register(
    SRCS "app_main.cpp" "camera_source.cpp" "preview.cpp"
    INCLUDE_DIRS "."
    REQUIRES gesture_core esp_timer esp_psram spi_flash esp32-camera
             hand_detect hand_gesture_recognition esp-dl esp_new_jpeg
             esp_wifi esp_event esp_netif esp_http_server nvs_flash)
target_compile_features(${COMPONENT_LIB} PRIVATE cxx_std_17)

