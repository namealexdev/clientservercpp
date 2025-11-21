#include "libinclude/stats.h"

string Stats::GetIP()
{
    return ipv4port;
}

void Stats::SetIP(const string &ip_str)
{
    strncpy(ipv4port, ip_str.c_str(), sizeof(ipv4port) - 1);
    ipv4port[sizeof(ipv4port) - 1] = '\0';
}

void Stats::AddRecvBytes(size_t bytes)
{
    recv_total_bytes += bytes;
}

void Stats::AddSendBytes(size_t bytes)
{
    send_total_bytes += bytes;
}

void Stats::CalcBitrate() {
    auto now = GetCurrentTimeMs();

    if (last_time == 0) {
        // Первый вызов - инициализируем
        last_time = now;
        recv_last_bytes = recv_total_bytes;
        send_last_bytes = send_total_bytes;
        recv_last_bps = 0;
        send_last_bps = 0;
        return;
    }

    uint64_t elapsed = now - last_time;
    if (elapsed == 0) return;

    // Расчет скорости приема
    uint64_t recv_delta = recv_total_bytes - recv_last_bytes;
    recv_last_bps = (recv_delta * 1000.0) / elapsed;

    // Расчет скорости отправки
    uint64_t send_delta = send_total_bytes - send_last_bytes;
    send_last_bps = (send_delta * 1000.0) / elapsed;

    // Обновляем состояние
    last_time = now;
    recv_last_bytes = recv_total_bytes;
    send_last_bytes = send_total_bytes;
}

double Stats::GetRecvBitrate() const { return recv_last_bps; }

double Stats::GetSendBitrate() const { return send_last_bps; }

double Stats::GetTotalBitrate() const { return recv_last_bps + send_last_bps; }

double Stats::GetTotalBitrateBits() const { return GetTotalBitrate() * 8.0; }

string Stats::GetFormattedBitrates() {
    CalcBitrate(); // Сначала обновляем расчет

    std::ostringstream oss;
    oss << "Recv: " << FormatBitrate(recv_last_bps)
        << " Send: " << FormatBitrate(send_last_bps)
        << " Total: " << FormatBitrate(GetTotalBitrate())
        << " (" << FormatBitrate(GetTotalBitrateBits(), false) << ")";
    return oss.str();
}

uint64_t Stats::GetRecvTotal() const
{
    return recv_total_bytes;
}

uint64_t Stats::GetSendTotal() const
{
    return send_total_bytes;
}

uint64_t Stats::GetTotalBytes() const
{
    return recv_total_bytes + send_total_bytes;
}

std::string Stats::FormatBitrate(double bps, bool bytes)
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

uint64_t Stats::GetCurrentTimeMs() const {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch()).count();
}

// double Stats::calcBitrate()
// {
//     auto now = std::chrono::steady_clock::now();
//     // Защита от первого вызова
//     if (last_time.time_since_epoch().count() == 0) {
//         last_time = now;
//         last_bytes = total_bytes;
//         return 0;
//     }

//     auto elapsed = std::chrono::duration_cast<std::chrono::duration<double>>(now - last_time).count();
//     if (elapsed <= 0.0) return 0;
//     uint64_t delta = total_bytes - last_bytes;

//     // Обновляем состояние
//     last_time = now;
//     last_bytes = total_bytes;

//     // double bps = (delta * 8.0) / elapsed; // bits per second
//     double btps = (delta) / elapsed; // bytes per second
//     last_bps = btps;
//     return last_bps;
// }
