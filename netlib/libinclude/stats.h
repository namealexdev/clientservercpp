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
    string ip;

    void addBytes(size_t bytes);

    // скорость за интервал с последнего вызова
    double calcBitrate();
    double getBitrate();
    string getCalcBitrate();

    static std::string formatBitrate(double bps, bool bytes = true);
    uint64_t total_bytes = 0;
private:

    uint64_t last_bytes = 0;
    std::chrono::steady_clock::time_point last_time{};
    double last_bps = 0;

    // false = bits

};

enum class EventType {
    ClientConnected,
    ClientDisconnected,
    DataReceived,

    WriteReady
};
struct DataReceived{
    int size;
    char* data;
};


#endif // STATS_H
