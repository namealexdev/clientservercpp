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
private:
    // Константы
    static constexpr uint64_t FOUR_GIB_BYTES = 4ULL * 1024 * 1024 * 1024; // 4 GiB
    static constexpr double NANOSECONDS_TO_SECONDS = 1e9;

    std::chrono::steady_clock::time_point start;

    // отслеживание 4 ГБайт
    uint64_t bytes_current_interval = 0;
    uint64_t packets_current_interval = 0;
    std::chrono::steady_clock::time_point interval_start_time; // Время начала текущего 4GB интервала

    // расчет с последнего вызова
    uint64_t last_bytes_for_bps = 0;
    uint64_t last_packets_for_pps = 0;
    std::chrono::steady_clock::time_point last_time_for_bps_pps;

public:
    // Обновление bps и pps
    void updateBps() {
        auto now = std::chrono::steady_clock::now();

        // Защита от первого вызова или если время не изменилось
        auto elapsed_ns = std::chrono::duration<double, std::nano>(now - last_time_for_bps_pps).count();
        if (elapsed_ns <= 0.0) {
            return;
        }

        uint64_t delta_bytes = bytes_current_interval - last_bytes_for_bps;
        uint64_t delta_packets = packets_current_interval - last_packets_for_pps;

        // Обновляем состояние
        last_bytes_for_bps = bytes_current_interval;
        last_packets_for_pps = packets_current_interval;
        last_time_for_bps_pps = now;

        // Рассчитываем bps и pps
        current_bps = (delta_bytes * 8.0) / (elapsed_ns / NANOSECONDS_TO_SECONDS);
        current_pps = static_cast<double>(delta_packets) / (elapsed_ns / NANOSECONDS_TO_SECONDS);
    }


    // Конструктор
    Stats() : interval_start_time(std::chrono::steady_clock::now()) {
        last_time_for_bps_pps = start = interval_start_time;
    }

    // IP клиента (для вывода)
    std::string ip;
    double current_bps = 0.0;
    double current_pps = 0.0;

    // Функция добавления байт (и пакета)
    void addBytes(size_t bytes) {
        bytes_current_interval += bytes;
        packets_current_interval += 1; // Увеличиваем счетчик пакетов на 1
    }

    // Функция проверки 4 ГБ и генерации сообщения
    // Возвращает true, если 4 ГБ были достигнуты и сообщение сгенерировано
    // Возвращает false, если 4 ГБ еще не достигнуто
    // Сообщение возвращается через параметр-ссылку message
    bool checkFourGigabytes(std::string& message) {
        if (bytes_current_interval >= FOUR_GIB_BYTES) {
            // 1. Рассчитываем время для 4GB сообщения
            auto now = std::chrono::steady_clock::now();
            auto elapsed_interval = std::chrono::duration<double>(now - interval_start_time).count();

            // 2. Формируем сообщение
            std::ostringstream oss;
            oss << "\n" << format_duration_since(interval_start_time)
                << " " << ip << " 4gb " << std::fixed << std::setprecision(2) << elapsed_interval << " s \n";
            message = oss.str();

            // 3. Сбрасываем интервал
            resetStats();

            return true; // Сообщение готово
        }
        return false; // 4 ГБ еще не достигнуто
    }

    void resetStats(){
        bytes_current_interval = 0;
        packets_current_interval = 0;
        interval_start_time = std::chrono::steady_clock::now();
        // Сбрасываем bps/pps счетчики тоже, чтобы не учитывать прошлые данные
        last_bytes_for_bps = 0;
        last_packets_for_pps = 0;
        last_time_for_bps_pps = interval_start_time; // или std::chrono::steady_clock::now()
        current_bps = 0.0;
        current_pps = 0.0;
    }

    string get_stats() {

        // Рассчитываем elapsed время с последнего обновления bps/pps
        // auto now = std::chrono::steady_clock::now();
        // auto elapsed_since_last_update = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_time_for_bps_pps).count();

        std::ostringstream oss;
        oss << format_duration_since(start) // Используем start для общего uptime
            << "\t" << ip
            << "\t" << formatValue(current_bps, "bps")
            << " " << formatValue(current_pps, "pps");
            // << " ("<< std::fixed << std::setprecision(3) << elapsed_since_last_update << "ms)";
        return oss.str();

        // return format_duration_since(start)
        //                           + "\t" + ip
        //                           + "\t" + formatValue(current_bps, "bps") + " " + formatValue(current_pps, "pps");
    }


    // Вспомогательная функция для форматирования битрейта/pps
    static std::string formatValue(double value, const std::string& unit) {
        if (value < 0) return "invalid";

        const char* units[] = {"", "K", "M", "G", "T"};
        size_t unit_index = 0;
        double v = value;

        while (v >= 1000.0 && unit_index < sizeof(units) / sizeof(units[0]) - 1) {
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
        auto centiseconds = (total_ms % 1000) / 10; // сотые доли секунды (0–99)

        std::ostringstream oss;
        oss << std::setfill('0')
            << std::setw(2) << hours << ":"
            << std::setw(2) << minutes << ":"
            << std::setw(2) << seconds << "."
            << std::setw(2) << centiseconds;

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
