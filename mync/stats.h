#ifndef STATS_H
#define STATS_H

#include <chrono>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <signal.h>
using namespace std;

uint64_t gcount_bytes = 0;
uint64_t gcount_packets = 0;
std::chrono::time_point<std::chrono::steady_clock> gstartup;


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

string getBitrate() {
    auto now = std::chrono::steady_clock::now();

    auto elapsed = std::chrono::duration_cast<std::chrono::duration<double>>(now - gstartup).count();
    if (elapsed <= 0.0) return "0";

    uint64_t delta = gcount_bytes - 0;

    double bps = (delta * 8.0) / elapsed; // bits per second
    double btps = (delta) / elapsed; // bytes per second
    return formatBitrate(btps) + " " + formatBitrate(bps, false);
}

void signal_handler(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        std::cout << "Closed stats: " << gcount_bytes << " bytes "
                  << gcount_packets << " packets speed: " << getBitrate() << std::endl;
        // keep_running = 0;
        exit(0);
    }
}

#endif // STATS_H
