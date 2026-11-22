#include "camera_source.hpp"
#include "preview.hpp"
#include "gesture_core.hpp"
#include "hand_detect.hpp"
#include "hand_gesture_recognition.hpp"
#include "esp_flash.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_psram.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <inttypes.h>
#include <list>
#include <string_view>

namespace {
constexpr const char *tag = "hand_gesture";
std::uint64_t now_ms() { return static_cast<std::uint64_t>(esp_timer_get_time() / 1000); }

void memory_report()
{
    ESP_LOGI(tag, "Internal free=%u, PSRAM free=%u, PSRAM largest=%u, stack free=%u bytes",
             static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)),
             static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)),
             static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM)),
             static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)));
}

void emit_selection(int digit, std::uint64_t timestamp)
{
    // Extension point for GPIO, UART or application-specific actions.
    // Called once per stable selection. Keep this function short and nonblocking.
    std::printf("{\"time_ms\":%" PRIu64 ",\"gesture\":%d,\"action\":\"selection_%d\"}\n",
                timestamp, digit, digit);
    std::fflush(stdout);
}
} // namespace

extern "C" void app_main()
{
    esp_log_level_set("*", ESP_LOG_WARN);
    esp_log_level_set(tag, ESP_LOG_INFO);
    if (!esp_psram_is_initialized() || esp_psram_get_size() < 8U * 1024U * 1024U) {
        ESP_LOGE(tag, "Requires at least 8 MiB PSRAM; check board and octal PSRAM settings.");
        return;
    }
    std::uint32_t flash_size = 0;
    if (esp_flash_get_size(nullptr, &flash_size) != ESP_OK || flash_size < 8U * 1024U * 1024U) {
        ESP_LOGE(tag, "Requires at least 8 MiB flash.");
        return;
    }
    CameraSource camera;
    const auto camera_error = camera.init();
    if (camera_error != ESP_OK) {
        ESP_LOGE(tag, "Camera init failed: %s; check camera board selection and ribbon cable.",
                 esp_err_to_name(camera_error));
        return;
    }
    HandDetect detector;
    HandGestureRecognizer recognizer;
    const auto preview_error = preview_start();
    if (preview_error == ESP_OK) {
        ESP_LOGI(tag, "Preview Wi-Fi: HandGesture-ESP32 | password: gesture123 | http://192.168.4.1");
    } else {
        ESP_LOGE(tag, "Preview unavailable: %s; recognition continues.", esp_err_to_name(preview_error));
    }
    gesture::Stabilizer filter({CONFIG_GESTURE_HOLD_MS, CONFIG_GESTURE_MAX_GAP_MS,
                                CONFIG_GESTURE_MIN_SAMPLES});
    ESP_LOGI(tag, "Ready: QVGA JPEG, local inference, one hand, digits 1..5.");
    memory_report();
    auto last_report = now_ms();
    unsigned capture_failures = 0;
    while (true) {
        const auto start = now_ms();
        dl::image::img_t image{};
        int digit = 0;
        float confidence = 0;
        const char *predicted = "no_hand";
        std::size_t hand_count = 0;
        if (camera.capture(image)) {
            capture_failures = 0;
            // ESP-DL owns its output list; copy only the accepted box to avoid
            // classifying multiple hands and to keep memory/latency bounded.
            auto &detections = detector.run(image);
            const float detection_threshold = CONFIG_GESTURE_DETECT_PERCENT / 100.0F;
            const auto count = std::count_if(detections.begin(), detections.end(),
                [detection_threshold](const auto &hand) {
                    return std::isfinite(hand.score) && hand.score >= detection_threshold;
                });
            hand_count = static_cast<std::size_t>(count);
            predicted = hand_count == 0 ? "no_hand" : (hand_count == 1 ? "unclassified" : "multiple_hands");
            if (hand_count == 1) {
                const auto hand = std::find_if(detections.begin(), detections.end(),
                    [detection_threshold](const auto &item) {
                        return std::isfinite(item.score) && item.score >= detection_threshold;
                    });
                auto box = *hand;
                if (box.box.size() >= 4) {
                    box.limit_box(image.width, image.height);
                    if (box.box[2] > box.box[0] && box.box[3] > box.box[1]) {
                        const std::list<dl::detect::result_t> single_hand{box};
                        const auto results = recognizer.recognize(image, single_hand);
                        if (results.size() == 1) {
                            confidence = results.front().score;
                            const auto *label = results.front().cat_name;
                            predicted = label ? label : "unknown";
                            digit = gesture::accepted_digit(label ? std::string_view(label) : std::string_view{},
                                confidence, hand_count, CONFIG_GESTURE_SCORE_PERCENT / 100.0F);
                        }
                    }
                }
            }
        } else if (++capture_failures >= 10) {
            ESP_LOGE(tag, "10 consecutive camera failures; stopping. Check hardware and reset.");
            return;
        }
        const auto finished = now_ms();
        const auto state = filter.update(digit, finished);
        preview_result(predicted, confidence, static_cast<unsigned>(hand_count), digit,
                       state.current, finished - start);
        if (state.event) emit_selection(state.event, finished);
        if (finished - last_report >= 5000) {
            ESP_LOGI(tag, "hands=%u class=%s raw=%d stable=%d confidence=%.2f cycle=%" PRIu64 "ms",
                     static_cast<unsigned>(hand_count), predicted, digit, state.current, confidence, finished - start);
            memory_report();
            last_report = finished;
        }
        // Yield to idle/system tasks. Watchdogs remain enabled.
        vTaskDelay(pdMS_TO_TICKS(CONFIG_GESTURE_FRAME_DELAY_MS));
    }
}
