#include "gesture_core.hpp"
#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

int main(int argc, char **argv)
{
    if (argc != 2) {
        std::cerr << "Usage: gesture_replay observations.csv\n"
                     "Rows: time_ms,label,confidence,hand_count\n";
        return 1;
    }
    std::ifstream input(argv[1]);
    if (!input) { std::cerr << "Cannot open input file\n"; return 1; }
    gesture::Stabilizer filter;
    std::string line;
    unsigned line_number = 0;
    std::int64_t previous = -1;
    while (std::getline(input, line)) {
        ++line_number;
        if (line.empty() || line.front() == '#') continue;
        std::replace(line.begin(), line.end(), ',', ' ');
        std::istringstream row(line);
        std::int64_t timestamp;
        int hands;
        float confidence;
        std::string label, trailing;
        if (!(row >> timestamp >> label >> confidence >> hands) || (row >> trailing) ||
            timestamp < 0 || timestamp <= previous || hands < 0) {
            std::cerr << "Invalid observation on line " << line_number << '\n';
            return 2;
        }
        previous = timestamp;
        const int digit = gesture::accepted_digit(label, confidence, static_cast<std::size_t>(hands));
        const auto result = filter.update(digit, static_cast<std::uint64_t>(timestamp));
        if (result.event) {
            std::cout << "{\"time_ms\":" << timestamp << ",\"gesture\":" << result.event
                      << ",\"action\":\"selection_" << result.event << "\"}\n";
        }
    }
    if (!input.eof()) { std::cerr << "Input read failed\n"; return 2; }
    return 0;
}

