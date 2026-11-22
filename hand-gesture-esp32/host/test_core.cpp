#include "gesture_core.hpp"
#include <cstdlib>
#include <iostream>
#include <limits>

namespace {
unsigned checks = 0;
void check(bool ok, const char *message)
{
    ++checks;
    if (!ok) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
void verify_mapping()
{
    const char *labels[] = {"one", "two", "three", "four", "five"};
    for (int i = 0; i < 5; ++i) {
        check(gesture::accepted_digit(labels[i], 0.9F, 1) == i + 1, "digit mapping");
        check(gesture::accepted_digit(labels[i], 0.74F, 1) == 0, "low confidence");
        check(gesture::accepted_digit(labels[i], 0.9F, 2) == 0, "multiple hands");
    }
    check(gesture::accepted_digit("five", 0.75F, 1) == 5, "threshold inclusive");
    check(gesture::accepted_digit("like", 0.99F, 1) == 0, "thumb-up is not one");
    check(gesture::accepted_digit("no_gesture", 0.99F, 1) == 0, "other gesture");
    check(gesture::accepted_digit("one", 0.99F, 0) == 0, "no hand");
    check(gesture::accepted_digit({}, 0.99F, 1) == 0, "empty label");
    check(gesture::accepted_digit("one", std::numeric_limits<float>::quiet_NaN(), 1) == 0, "NaN");
    check(gesture::accepted_digit("one", std::numeric_limits<float>::infinity(), 1) == 0, "infinity");
    check(gesture::accepted_digit("one", 1.1F, 1) == 0, "invalid probability");
}
void verify_timing()
{
    gesture::Stabilizer f;
    check(f.update(4, 0).event == 0, "first sample");
    check(f.update(4, 300).event == 0, "short hold");
    auto result = f.update(4, 600);
    check(result.event == 4 && result.current == 4, "four confirmed");
    check(f.update(4, 900).event == 0, "held pose not repeated");
    check(f.update(5, 1000).event == 0, "transient five");
    check(f.update(4, 1100).event == 0, "return to previous pose");
    check(f.update(0, 1200).current == 4, "brief loss retains selection");
    check(f.update(4, 1300).event == 0, "brief loss does not rearm");
    f.update(0, 1400);
    f.update(0, 1700);
    result = f.update(0, 2000);
    check(result.current == 0 && result.event == 0, "stable release");
    f.update(4, 2100);
    f.update(4, 2400);
    check(f.update(4, 2700).event == 4, "same digit after release");
    f.update(5, 2800);
    f.update(5, 3100);
    check(f.update(5, 3400).event == 5, "direct transition to five");
    f.reset();
    f.update(1, 0);
    check(f.update(1, 600).event == 0, "time alone is insufficient");
    check(f.update(1, 600).event == 0, "same timestamp is not new evidence");
    check(f.update(1, 601).event == 1, "third distinct sample");
    check(f.update(1, 3000).current == 0, "camera stall resets stale state");
    check(f.update(1, 3001).event == 0, "stall needs new hold");
    check(f.update(1, 3600).event == 1, "recover after stall");
    check(f.update(1, 10).current == 0, "clock regression resets");
    f.update(99, 20);
    f.update(99, 320);
    check(f.update(99, 620).current == 0, "invalid digit ignored");
}
} // namespace

int main()
{
    verify_mapping();
    verify_timing();
    std::cout << checks << " checks passed\n";
}
