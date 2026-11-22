#pragma once
#include "dl_image_define.hpp"
#include "esp_camera.h"
#include "esp_jpeg_dec.h"

// Owns one aligned RGB buffer in PSRAM. The returned view lasts until next capture.
class CameraSource {
public:
    static constexpr unsigned width = 320;
    static constexpr unsigned height = 240;
    CameraSource() = default;
    ~CameraSource();
    CameraSource(const CameraSource &) = delete;
    CameraSource &operator=(const CameraSource &) = delete;
    esp_err_t init();
    bool capture(dl::image::img_t &image);

private:
    bool initialized_ = false;
    jpeg_dec_handle_t decoder_ = nullptr;
    unsigned char *rgb_ = nullptr;
};

