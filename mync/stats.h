#ifndef STATS_H
#define STATS_H

#include <chrono>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <signal.h>
using namespace std;

// uint64_t gcount_bytes = 0;
// uint64_t gcount_packets = 0;
// std::chrono::time_point<std::chrono::steady_clock> gstartup;

#include <chrono>
#include <cstdint>
#include <iomanip>
#include <sstream>

class Stats {
public:
    std::string ip;

    void addBytes(size_t bytes) {
        total_bytes += bytes;
    }

    //  скорость за интервал с последнего вызова
    string getBitrate() {
        auto now = std::chrono::steady_clock::now();

        // Защита от первого вызова
        if (last_time.time_since_epoch().count() == 0) {
            last_time = now;
            last_bytes = total_bytes;
            return "0";
        }

        auto elapsed = std::chrono::duration_cast<std::chrono::duration<double>>(now - last_time).count();
        if (elapsed <= 0.0) return "0";

        uint64_t delta = total_bytes - last_bytes;

        // Обновляем состояние
        last_time = now;
        last_bytes = total_bytes;

        double bps = (delta * 8.0) / elapsed; // bits per second
        double btps = (delta) / elapsed; // bytes per second
        return formatBitrate(btps) + " " + formatBitrate(bps, false);
    }

private:
    uint64_t total_bytes;
    uint64_t last_bytes = 0;
    std::chrono::steady_clock::time_point last_time{};

    std::string formatBitrate(double bps, bool bytes = true) // false = bits
    {
        if (bps < 0) return "invalid";

        const char* byte_units[] = {"bytes", "Kbytes", "Mbytes", "Gbytes", "Tbytes"};
        const char* bitrate_units[] = {"bits/s", "Kbits/s", "Mbits/s", "Gbits/s", "Tbits/s"};
        const char** units;
        if (bytes){
            units = byte_units;
        }else{
            units = bitrate_units;
        }

        size_t unit_index = 0;
        double value = bps;

        while (value >= 1000.0 && unit_index < sizeof(byte_units) / sizeof(byte_units[0]) - 1) {
            value /= 1000.0;
            ++unit_index;
        }

        std::ostringstream oss;
        oss << std::fixed << std::setprecision(2) << value << " " << units[unit_index];
        return oss.str();
    }
};

void signal_handler(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        // std::cout << "Closed stats: " << gcount_bytes << " bytes "
        //           << gcount_packets << " packets speed: " << getBitrate() << std::endl;
        // keep_running = 0;
        exit(0);
    }
}

#endif // STATS_H
