#ifndef STATS_H
#define STATS_H

#include <chrono>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <cstdint>
#include <sstream>

class Stats {
private:
    // static constexpr uint64_t FOUR_GiB = 4ULL * 1024 * 1024 * 1024;
    static constexpr uint64_t FOUR_GB = 4ULL * 1000 * 1000 * 1000;

public:
    std::chrono::steady_clock::time_point start_time;

    // для битрейта
    uint64_t total_bytes = 0;
    uint64_t last_update_bytes = 0;
    std::chrono::steady_clock::time_point last_update_time;
    // uint64_t total_packets = 0;

    // для определения 4gb
    uint64_t interval_bytes = 0;
    std::chrono::steady_clock::time_point last_interval_time;

    std::string ip;
    double current_bps = 0.0;
    // double current_pps = 0.0;

    Stats() {
        last_update_time = last_interval_time = start_time = std::chrono::steady_clock::now();
    }

    void addBytes(size_t bytes) {
        total_bytes += bytes;
        // total_packets += 1;
        interval_bytes += bytes;
    }

    void updateBps() {
        auto now = std::chrono::steady_clock::now();
        auto elapsed_ns = std::chrono::duration<double, std::nano>(now - last_update_time).count();

        if (elapsed_ns <= 0) return;

        uint64_t delta_bytes = total_bytes - last_update_bytes;

        current_bps = (delta_bytes * 8.0) / (elapsed_ns / 1e9);
        // current_pps = static_cast<double>(total_packets) / (std::chrono::duration<double>(now - last_update_time).count());

        last_update_bytes = total_bytes;
        last_update_time = now;
    }

    bool checkFourGigabytes(std::string& message) {
        if (interval_bytes >= FOUR_GB) {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration<double>(now - last_interval_time).count();
            // auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_interval_time).count();
            last_interval_time = now;

            double estimated_time_for_4gb = (static_cast<double>(FOUR_GB) * elapsed) / static_cast<double>(interval_bytes);


            std::ostringstream oss;
            oss << "\n" << format_duration_since(start_time) << " " << ip << " "  << std::fixed << std::setprecision(2)
                << interval_bytes/1e9 << "GB " << elapsed << "s"
                << " (4gb ~" << estimated_time_for_4gb << "s) "
                << "\n";
            message = oss.str();

            // Сбрасываем интервал
            interval_bytes = 0;
            return true;
        }
        return false;
    }

    std::string get_stats() const {
        std::ostringstream oss;
        oss << format_duration_since(start_time)
            << "\t" << ip
            << "\t" << formatValue(current_bps, "bps");
            // << " " << formatValue(current_pps, "pps");
        return oss.str();
    }

    static std::string formatValue(double value, const std::string& unit) {
        if (value < 0) return "invalid";

        const char* units[] = {"", "k", "m", "g", "t"};
        size_t unit_index = 0;
        double v = value;

        while (v >= 1000.0 && unit_index < 4) {
            v /= 1000.0;
            ++unit_index;
        }

        std::ostringstream oss;
        oss << std::fixed << std::setprecision(2) << v << " " << units[unit_index] << unit;
        return oss.str();
    }

    static std::string format_duration_since(std::chrono::steady_clock::time_point start_time) {
        auto now = std::chrono::steady_clock::now();
        auto dur = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time);

        auto total_ms = dur.count();
        auto hours = total_ms / (3600 * 1000);
        total_ms %= (3600 * 1000);
        auto minutes = total_ms / (60 * 1000);
        total_ms %= (60 * 1000);
        auto seconds = total_ms / 1000;
        auto centiseconds = (total_ms % 1000) / 10;

        std::ostringstream oss;
        oss << std::setfill('0')
            << std::setw(2) << hours << ":"
            << std::setw(2) << minutes << ":"
            << std::setw(2) << seconds << "."
            << std::setw(2) << centiseconds;
        return oss.str();
    }
};

#endif // STATS_H
