cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(hand_gesture_esp32)
if(NOT IDF_TARGET STREQUAL "esp32s3")
    message(FATAL_ERROR "This firmware requires ESP32-S3, not the original ESP32-CAM.")
endif()

