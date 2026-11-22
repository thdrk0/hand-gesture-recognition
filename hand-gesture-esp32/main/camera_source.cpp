#include "camera_source.hpp"
#include "preview.hpp"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include <limits>

namespace {
constexpr const char *tag = "gesture_camera";
struct Pins {
    int xclk, sda, scl, d0, d1, d2, d3, d4, d5, d6, d7, vsync, href, pclk;
};
// D0..D7 corresponds to Y2..Y9 in the board schematic.
#if CONFIG_GESTURE_BOARD_S3_EYE
constexpr Pins pins{15, 4, 5, 11, 9, 8, 10, 12, 18, 17, 16, 6, 7, 13};
#elif CONFIG_GESTURE_BOARD_XIAO_SENSE
constexpr Pins pins{10, 40, 39, 15, 17, 18, 16, 14, 12, 11, 48, 38, 47, 13};
#else
#error "Select a supported camera board in menuconfig"
#endif

struct FrameGuard {
    camera_fb_t *frame;
    ~FrameGuard() { if (frame) esp_camera_fb_return(frame); }
};
} // namespace

CameraSource::~CameraSource()
{
    if (decoder_) jpeg_dec_close(decoder_);
    if (initialized_) esp_camera_deinit();
    heap_caps_free(rgb_);
}

esp_err_t CameraSource::init()
{
    if (initialized_ || rgb_ || decoder_) return ESP_ERR_INVALID_STATE;
    rgb_ = static_cast<unsigned char *>(heap_caps_aligned_alloc(
        16, width * height * 3, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!rgb_) return ESP_ERR_NO_MEM;

    jpeg_dec_config_t jpeg_config{};
    jpeg_config.output_type = JPEG_PIXEL_FORMAT_RGB888;
    jpeg_config.rotate = JPEG_ROTATE_0D;
    if (jpeg_dec_open(&jpeg_config, &decoder_) != JPEG_ERR_OK) return ESP_FAIL;

    camera_config_t config{};
    config.pin_pwdn = -1;
    config.pin_reset = -1;
    config.pin_xclk = pins.xclk;
    config.pin_sccb_sda = pins.sda;
    config.pin_sccb_scl = pins.scl;
    config.pin_d0 = pins.d0;
    config.pin_d1 = pins.d1;
    config.pin_d2 = pins.d2;
    config.pin_d3 = pins.d3;
    config.pin_d4 = pins.d4;
    config.pin_d5 = pins.d5;
    config.pin_d6 = pins.d6;
    config.pin_d7 = pins.d7;
    config.pin_vsync = pins.vsync;
    config.pin_href = pins.href;
    config.pin_pclk = pins.pclk;
    config.xclk_freq_hz = 20000000;
    config.ledc_timer = LEDC_TIMER_0;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.pixel_format = PIXFORMAT_JPEG;
    config.frame_size = FRAMESIZE_QVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
    config.fb_location = CAMERA_FB_IN_PSRAM;
    config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
    const auto err = esp_camera_init(&config);
    if (err != ESP_OK) return err;
    initialized_ = true;
    auto *sensor = esp_camera_sensor_get();
    if (!sensor) return ESP_FAIL;
#ifdef CONFIG_GESTURE_CAMERA_VFLIP
    if (sensor->set_vflip(sensor, 1) != 0) return ESP_FAIL;
#endif
#ifdef CONFIG_GESTURE_CAMERA_HMIRROR
    if (sensor->set_hmirror(sensor, 1) != 0) return ESP_FAIL;
#endif
    return ESP_OK;
}

bool CameraSource::capture(dl::image::img_t &image)
{
    image = {};
    FrameGuard guard{esp_camera_fb_get()};
    auto *fb = guard.frame;
    if (!fb || fb->format != PIXFORMAT_JPEG || fb->width != width || fb->height != height ||
        fb->len == 0 || fb->len > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        ESP_LOGW(tag, "Missing frame or unexpected camera format/size");
        return false;
    }
    jpeg_dec_io_t io{};
    io.inbuf = fb->buf;
    io.inbuf_len = static_cast<int>(fb->len);
    jpeg_dec_header_info_t info{};
    if (jpeg_dec_parse_header(decoder_, &io, &info) != JPEG_ERR_OK ||
        info.width != width || info.height != height) {
        ESP_LOGW(tag, "Invalid JPEG header or dimensions");
        return false;
    }
    int required_bytes = 0;
    if (jpeg_dec_get_outbuf_len(decoder_, &required_bytes) != JPEG_ERR_OK ||
        required_bytes <= 0 || static_cast<unsigned>(required_bytes) > width * height * 3) {
        ESP_LOGW(tag, "JPEG output exceeds buffer capacity");
        return false;
    }
    // Dimensions and decoder output size checked before writing the fixed buffer.
    io.outbuf = rgb_;
    if (jpeg_dec_process(decoder_, &io) != JPEG_ERR_OK) {
        ESP_LOGW(tag, "JPEG decode failed");
        return false;
    }
    image.data = rgb_;
    image.width = width;
    image.height = height;
    image.pix_type = dl::image::DL_IMAGE_PIX_TYPE_RGB888;
    preview_frame(fb->buf, fb->len);
    return true; // FrameGuard returns compressed buffer before inference.
}

