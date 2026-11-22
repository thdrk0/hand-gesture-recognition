#pragma once
#include <cstddef>
#include <cstdint>
#include "esp_err.h"

esp_err_t preview_start();
void preview_frame(const std::uint8_t *jpeg, std::size_t size);
void preview_result(const char *label, float confidence, unsigned hands,
                    int digit, int stable, std::uint64_t cycle_ms);
