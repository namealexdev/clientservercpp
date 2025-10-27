#ifndef SNOWFLAKEIDGEN_H
#define SNOWFLAKEIDGEN_H

#include <cstdint>
#include <chrono>
#include <mutex>
#include <thread>
#include <stdexcept>

// --- Настройки и состояние генератора ---
constexpr uint64_t EPOCH_START_MS = 1735689600000ULL; // 1 января 2025 г.
constexpr uint16_t WORKER_ID = 42;                    // Уникальный ID текущего сервера (0-1023) (10 бит)

// КОЛИЧЕСТВО БИТОВ ДЛЯ СЧЕТЧИКА (Sequence Counter):
constexpr int SEQUENCE_BITS = 13; // 13 бит
/* * Если SEQUENCE_BITS БОЛЬШЕ (например, 15):
 * + Увеличится максимальная пропускная способность сервера с 8192 до 32768 ID/мс.
 * - Уменьшится количество битов для WORKER_ID или Timestamp, что сократит срок жизни ID или число серверов.
 * Если SEQUENCE_BITS МЕНЬШЕ (например, 10):
 * - Уменьшится максимальная пропускная способность сервера с 8192 до 1024 ID/мс.
 * + Увеличится количество битов для WORKER_ID или Timestamp.
*/

// КОЛИЧЕСТВО БИТОВ ДЛЯ ID СЕРВЕРА (Worker ID):
constexpr int WORKER_ID_BITS = 10; // 10 бит
/*
 * Если WORKER_ID_BITS БОЛЬШЕ (например, 12):
 * + Увеличится максимальное число серверов в кластере с 1024 до 4096.
 * - Уменьшится количество битов для SEQUENCE_BITS или Timestamp.
 * Если WORKER_ID_BITS МЕНЬШЕ (например, 8):
 * - Уменьшится максимальное число серверов в кластере с 1024 до 256.
 * + Увеличится количество битов для SEQUENCE_BITS или Timestamp.
*/

// --- ТЕХНИЧЕСКИЕ КОНСТАНТЫ (Сдвиги и Маски) ---
constexpr uint64_t MAX_SEQUENCE = (1ULL << SEQUENCE_BITS) - 1; // 8191
constexpr uint64_t WORKER_ID_SHIFT = SEQUENCE_BITS; // Сдвиг для Worker ID
constexpr uint64_t TIMESTAMP_SHIFT = SEQUENCE_BITS + WORKER_ID_BITS; // Сдвиг для Timestamp


// constexpr uint64_t EPOCH_START_MS = 1735689600000ULL; // 1 января 2025 г.
// constexpr uint64_t WORKER_ID = 42;                    // 0-1023 (10 бит)
// constexpr int SEQUENCE_BITS = 13;                     // 13 бит
// constexpr int WORKER_ID_BITS = 10;                    // 10 бит

// constexpr uint64_t MAX_SEQUENCE = (1ULL << SEQUENCE_BITS) - 1; // 8191
// constexpr uint64_t WORKER_ID_SHIFT = SEQUENCE_BITS;            // Сдвиг для Worker ID
// constexpr uint64_t TIMESTAMP_SHIFT = SEQUENCE_BITS + WORKER_ID_BITS; // Сдвиг для Timestamp

class SnowflakeGenerator {
private:
    std::mutex mtx_;
    uint64_t last_timestamp_ms_ = 0;
    uint64_t sequence_ = 0;

    // Вспомогательная функция для получения текущего времени в мс
    uint64_t time_gen_ms() {
        auto now = std::chrono::time_point_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now()
            );
        return now.time_since_epoch().count();
    }

    // Блокировка до перехода на следующую миллисекунду
    uint64_t wait_for_next_ms(uint64_t last_ts) {
        uint64_t timestamp = time_gen_ms();
        while (timestamp <= last_ts) {
            std::this_thread::yield(); // Уступаем процессорное время
            timestamp = time_gen_ms();
        }
        return timestamp;
    }

public:
    uint64_t next_id() {
        std::lock_guard<std::mutex> lock(mtx_); // Защищаем все общие переменные

        uint64_t timestamp = time_gen_ms();

        // 1. Обработка "времени назад" (часы сдвинулись)
        if (timestamp < last_timestamp_ms_) {
            throw std::runtime_error("Clock moved backwards. Refusing to generate ID for "
                                     + std::to_string(last_timestamp_ms_ - timestamp) + "ms");
        }

        // 2. Обработка в той же миллисекунде
        if (timestamp == last_timestamp_ms_) {
            sequence_ = (sequence_ + 1) & MAX_SEQUENCE;

            // Если счетчик переполнился (сгенерировано > 8192 ID за мс)
            if (sequence_ == 0) {
                timestamp = wait_for_next_ms(last_timestamp_ms_); // ⏳ Ждем следующую мс
            }
        }
        // 3. Обработка перехода на новую миллисекунду
        else {
            sequence_ = 0; // Начинаем счетчик с нуля
        }

        // 4. Обновление состояния и сборка ID
        last_timestamp_ms_ = timestamp;

        uint64_t relative_timestamp = timestamp - EPOCH_START_MS;

        // Сборка 64-битного ID
        uint64_t id = (relative_timestamp << TIMESTAMP_SHIFT) |
                      (WORKER_ID << WORKER_ID_SHIFT) |
                      sequence_;

        return id;
    }
};

/*
#include <vector>
#include <iostream>
int main() {
    SnowflakeGenerator generator;

    std::vector<uint64_t> ids;
    for (int i = 0; i < 5; ++i) {
        ids.push_back(generator.next_id());
        // Добавление небольшой задержки для демонстрации разных таймстемпов
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    std::cout << "Generated IDs:" << std::endl;
    for (uint64_t id : ids) {
        std::cout << id << std::endl;
    }
    return 0;
}
*/

#endif // SNOWFLAKEIDGEN_H
