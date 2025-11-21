#ifndef STATS_H
#define STATS_H

#include <chrono>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <string>
#include "eventtype.h"

using std::string;

class Stats {
public:
    std::string GetIP();
    void SetIP(const std::string& ip_str);

    void AddRecvBytes(size_t bytes);
    void AddSendBytes(size_t bytes);

    // Общая функция расчета скоростей для приема и отправки
    void CalcBitrate();

    // Получение скоростей (после вызова CalcBitrate)
    double GetRecvBitrate() const;      // байт/сек приема
    double GetSendBitrate() const;      // байт/сек отправки
    double GetTotalBitrate() const;  // общая байт/сек
    double GetTotalBitrateBits() const;    // общая бит/сек

    // Расчет и получение всех скоростей в отформатированном виде
    std::string GetFormattedBitrates();

    // Получение статистики
    uint64_t GetRecvTotal() const;
    uint64_t GetSendTotal() const;
    uint64_t GetTotalBytes() const;

    static std::string FormatBitrate(double bps, bool bytes = true);

private:
    uint64_t GetCurrentTimeMs() const;
    uint64_t recv_total_bytes = 0;
    uint64_t recv_last_bytes = 0;
    double recv_last_bps = 0;

    uint64_t send_total_bytes = 0;
    uint64_t send_last_bytes = 0;
    double send_last_bps = 0;

    uint64_t last_time = 0;

    // string ipv4port;
    char ipv4port[32] = {0};
};


#endif // STATS_H
