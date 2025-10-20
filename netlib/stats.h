#ifndef STATS_H
#define STATS_H

#include "const.h"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <sstream>

class Stats {
public:
    void addBytes(size_t bytes) {
        total_bytes.fetch_add(bytes, std::memory_order_relaxed);
    }

    //  скорость за интервал с последнего вызова этой функции
    string getBitrate() {
        auto now = std::chrono::steady_clock::now();
        uint64_t current_bytes = total_bytes.load(std::memory_order_relaxed);

        // Защита от первого вызова
        if (last_time.time_since_epoch().count() == 0) {
            last_time = now;
            last_bytes = current_bytes;
            return "0";
        }

        auto elapsed = std::chrono::duration_cast<std::chrono::duration<double>>(now - last_time).count();
        if (elapsed <= 0.0) return "0";

        uint64_t delta = current_bytes - last_bytes;

        // Обновляем состояние
        last_time = now;
        last_bytes = current_bytes;

        double bps = (delta * 8.0) / elapsed; // bits per second
        return formatBitrate(bps);
    }

private:
    std::atomic<uint64_t> total_bytes{0};

    uint64_t last_bytes = 0;
    std::chrono::steady_clock::time_point last_time{};

    std::string formatBitrate(double bps) {
        if (bps < 0) return "invalid";

        const char* units[] = {"bps", "Kbps", "Mbps", "Gbps", "Tbps"};
        size_t unit_index = 0;
        double value = bps;

        while (value >= 1000.0 && unit_index < sizeof(units) / sizeof(units[0]) - 1) {
            value /= 1000.0;
            ++unit_index;
        }

        std::ostringstream oss;
        oss << std::fixed << std::setprecision(2) << value << " " << units[unit_index];
        return oss.str();
    }
};

#endif // STATS_H
