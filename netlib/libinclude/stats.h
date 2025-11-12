#ifndef STATS_H
#define STATS_H

#include <chrono>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>

using std::string;

class Stats {
public:
    std::string ip;
    double last_bps = 0;

    void addBytes(size_t bytes);

    // скорость за интервал с последнего вызова
    string getBitrate();

private:
    uint64_t total_bytes;
    uint64_t last_bytes = 0;
    std::chrono::steady_clock::time_point last_time{};

    // false = bits
    std::string formatBitrate(double bps, bool bytes = true);
};

enum class EventType {
    ClientConnected,
    ClientDisconnected,
    DataReceived
};
struct DataReceived{
    int size;
    char* data;
};


#endif // STATS_H
