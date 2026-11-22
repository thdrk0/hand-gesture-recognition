#include "gesture_core.hpp"
#include <cmath>
#include <limits>

namespace gesture {
int accepted_digit(std::string_view label, float confidence, std::size_t hands, float threshold)
{
    if (hands != 1 || !std::isfinite(confidence) || confidence < 0 || confidence > 1 ||
        !std::isfinite(threshold) || threshold < 0 || threshold > 1 || confidence < threshold) {
        return 0;
    }
    constexpr std::string_view labels[] = {"one", "two", "three", "four", "five"};
    for (int i = 0; i < 5; ++i) {
        if (label == labels[i]) return i + 1;
    }
    return 0;
}

Stabilizer::Stabilizer(FilterConfig config) : config_(config)
{
    if (config_.min_samples < 1) config_.min_samples = 1;
}

void Stabilizer::reset()
{
    started_ = false;
    candidate_ = current_ = 0;
    samples_ = 0;
    since_ = last_ = 0;
}

Update Stabilizer::update(int digit, std::uint64_t now_ms)
{
    if (digit < 1 || digit > 5) digit = 0;
    // Do not confirm a pose using samples separated by a camera stall.
    if (started_ && (now_ms < last_ || now_ms - last_ > config_.max_gap_ms)) reset();
    if (!started_ || digit != candidate_) {
        candidate_ = digit;
        since_ = now_ms;
        samples_ = 1;
    } else if (now_ms > last_ && samples_ < std::numeric_limits<unsigned>::max()) {
        ++samples_;
    }
    started_ = true;
    last_ = now_ms;
    int event = 0;
    if (now_ms - since_ >= config_.hold_ms && samples_ >= config_.min_samples &&
        current_ != candidate_) {
        current_ = candidate_;
        event = current_;
    }
    return {current_, event};
}
} // namespace gesture

