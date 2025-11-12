#include "libinclude/stats.h"

void Stats::addBytes(size_t bytes)
{
    total_bytes += bytes;
}

std::string Stats::getBitrate()
{
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
    last_bps = btps;
    return formatBitrate(btps) + " " + formatBitrate(bps, false);
}

std::string Stats::formatBitrate(double bps, bool bytes)
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
