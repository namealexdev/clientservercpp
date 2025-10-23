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
    std::chrono::time_point<std::chrono::steady_clock> start;
public:

    Stats(){
        start = last_time = std::chrono::steady_clock::now();
        update_bitrate();
    }
    std::string ip;
    double bps = 0.0;
    uint64_t total_bytes = 0;

    void addBytes(size_t bytes) {
        total_bytes += bytes;
    }

    // каждые 4gb считаем битрейт, скидываем total в 0
    bool is4gb(){
        const uint64_t FOUR_GIB = 4ULL * 1024 * 1024 * 1024; // 4 GiB = 4 * 2^30
        // std::cout << "is4 " << total_bytes << std::endl;
        update_bitrate();
        if (total_bytes >= FOUR_GIB){

            total_bytes -= FOUR_GIB;
            return true;
        }
        return false;
    }

    string get_stats(){
        std::cout << " get_stats " << bps << std::endl;
        std::stringstream s;
        s << "\n" << format_duration_since(start) << "\t" << ip << " " << formatBitrate(bps);
        return s.str();
    }

    //  скорость за интервал с последнего вызова
    void update_bitrate() {
        auto now = std::chrono::steady_clock::now();

        // Защита от первого вызова
        if (last_time.time_since_epoch().count() == 0) {
            last_time = now;
            last_bytes = total_bytes;
            return ;//"0";
        }

        auto elapsed = std::chrono::duration_cast<std::chrono::duration<double>>(now - last_time).count();
        std::cout << "\n elapsed" << elapsed << "\n";
        if (elapsed <= 0.0) return ;//"0";

        uint64_t delta = total_bytes - last_bytes;

        // Обновляем состояние
        last_time = now;
        last_bytes = total_bytes;

        bps = (delta * 8.0) / elapsed; // bits per second
        std::cout << "delta:" << delta << " el:" << elapsed << " bps:" << bps << std::endl;
        // return formatBitrate(bps);
        // double btps = (delta) / elapsed; // bytes per second
        // return formatBitrate(btps, true) + " " + formatBitrate(bps, false);
    }

private:

    uint64_t last_bytes = 0;
    std::chrono::steady_clock::time_point last_time{};

    std::string format_duration_since(std::chrono::steady_clock::time_point start) {
        auto now = std::chrono::steady_clock::now();
        auto dur = std::chrono::duration_cast<std::chrono::milliseconds>(now - start);

        auto total_ms = dur.count();
        auto hours = total_ms / (3600 * 1000);
        total_ms %= (3600 * 1000);
        auto minutes = total_ms / (60 * 1000);
        total_ms %= (60 * 1000);
        auto seconds = total_ms / 1000;
        auto centiseconds = (total_ms % 1000) / 10; // сотые доли секунды (0–99)

        std::ostringstream oss;
        oss << std::setfill('0')
            << std::setw(2) << hours << ":"
            << std::setw(2) << minutes << ":"
            << std::setw(2) << seconds << "."
            << std::setw(2) << centiseconds;

        return oss.str();
    }
public:
    /// false = bits
    static std::string formatBitrate(double bps, bool bytes = true)
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
