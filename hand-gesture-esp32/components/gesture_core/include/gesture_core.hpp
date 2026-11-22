#pragma once
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace gesture {
// 0 means no accepted digit (no hand, ambiguous or unsupported gesture).
int accepted_digit(std::string_view label, float confidence, std::size_t hands,
                   float threshold = 0.75F);

struct FilterConfig {
    std::uint64_t hold_ms = 600;
    std::uint64_t max_gap_ms = 1500;
    unsigned min_samples = 3;
};

struct Update {
    int current;
    int event; // Nonzero only once on each stable transition to 1..5.
};

class Stabilizer {
public:
    explicit Stabilizer(FilterConfig config = {});
    Update update(int digit, std::uint64_t now_ms);
    void reset();

private:
    FilterConfig config_;
    bool started_ = false;
    int candidate_ = 0;
    int current_ = 0;
    unsigned samples_ = 0;
    std::uint64_t since_ = 0;
    std::uint64_t last_ = 0;
};
} // namespace gesture

