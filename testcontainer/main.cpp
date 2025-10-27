#include <iostream>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <algorithm>
#include <random>
#include <numeric>

#include <iostream>
#include <map>
#include <atomic>
#include <sys/socket.h>
#include <unistd.h>
#include <cstdint>


// 2. Глобальный счетчик для обеспечения уникальности ID
// std::atomic<uint64_t> global_connection_counter{0};
//     uint64_t new_app_id = global_connection_counter.fetch_add(1) + 1;

// simple_flat_map
template<typename Value>
class simple_flat_map {
public:
    struct Entry {
        uint64_t key = -1;
        bool occupied = false;
        Value value;
    };

    explicit simple_flat_map(size_t capacity = 1024) : entries(capacity) {}

    void insert(uint64_t key, Value val) {
        size_t index = hash(key);
        size_t start = index;
        while (entries[index].occupied && entries[index].key != key) {
            index = (index + 1) % entries.size();
            if (index == start) {
                // Table is full, need to resize (not implemented for simplicity)
                return;
            }
        }
        entries[index].key = key;
        entries[index].value = std::move(val);
        entries[index].occupied = true;
    }

    Value* find(uint64_t key) {
        size_t index = hash(key);
        size_t start = index;
        do {
            if (!entries[index].occupied) return nullptr;
            if (entries[index].key == key && entries[index].occupied) {
                return &entries[index].value;
            }
            index = (index + 1) % entries.size();
        } while (index != start);
        return nullptr;
    }

private:
    std::vector<Entry> entries;

    size_t hash(uint64_t key) const {
        return static_cast<size_t>(key) % entries.size();
    }
};

struct SocketData {
    size_t timestamp;
    int data1, data2, data3, data4;
    SocketData() : timestamp(0), data1(0), data2(0), data3(0), data4(0) {}
};

// Генерация fd как в реальном сценарии - постепенно приходящие соединения
std::vector<int> generate_sequential_fds(int num_fds) {
    std::vector<int> fds(num_fds);
    std::iota(fds.begin(), fds.end(), 3); // Начинаем с 3 (после stdin, stdout, stderr)
    return fds;
}

std::vector<int> generate_random_fds(int num_fds) {
    std::vector<int> fds(num_fds);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(100, 1000000);
    for (int i = 0; i < num_fds; ++i) {
        fds[i] = dis(gen);
    }
    return fds;
}

double median(std::vector<double>& times) {
    std::sort(times.begin(), times.end());
    size_t n = times.size();
    return (n % 2 == 0) ? (times[n/2 - 1] + times[n/2]) / 2.0 : times[n/2];
}

// Реалистичный вектор - храним пары (fd, data) и используем линейный поиск
class VectorSocketStore {
private:
    std::vector<std::pair<int, SocketData>> data;

public:
    void insert(int fd, SocketData socket_data) {
        // В реальности fd добавляются постепенно, обычно в конец
        data.emplace_back(fd, std::move(socket_data));
    }

    SocketData* find(int fd) {
        // Линейный поиск - самый простой, но неэффективный для больших объемов
        for (auto& pair : data) {
            if (pair.first == fd) {
                return &pair.second;
            }
        }
        return nullptr;
    }

    size_t size() const { return data.size(); }
};

// Оптимизированный вектор - поддерживаем отсортированный массив для бинарного поиска
class SortedVectorSocketStore {
private:
    std::vector<std::pair<int, SocketData>> data;
    bool sorted = true;

public:
    void insert(int fd, SocketData socket_data) {
        data.emplace_back(fd, std::move(socket_data));
        sorted = false; // Помечаем, что массив нужно пересортировать
    }

    SocketData* find(int fd) {
        // Если массив не отсортирован, сортируем его
        if (!sorted) {
            std::sort(data.begin(), data.end(),
                      [](const auto& a, const auto& b) { return a.first < b.first; });
            sorted = true;
        }

        // Бинарный поиск
        auto it = std::lower_bound(data.begin(), data.end(), fd,
                                   [](const auto& pair, int fd) { return pair.first < fd; });
        if (it != data.end() && it->first == fd) {
            return &it->second;
        }
        return nullptr;
    }

    size_t size() const { return data.size(); }
};

// Бенчмарки для постепенного добавления (имитация реального accept)
double benchmark_gradual_insert_vector(const std::vector<int>& fds) {
    VectorSocketStore store;

    auto start = std::chrono::high_resolution_clock::now();
    for (int fd : fds) {
        store.insert(fd, SocketData{});
    }
    auto end = std::chrono::high_resolution_clock::now();
    return std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count() / (double)fds.size();
}

double benchmark_gradual_insert_sorted_vector(const std::vector<int>& fds) {
    SortedVectorSocketStore store;

    auto start = std::chrono::high_resolution_clock::now();
    for (int fd : fds) {
        store.insert(fd, SocketData{});
    }
    auto end = std::chrono::high_resolution_clock::now();
    return std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count() / (double)fds.size();
}

double benchmark_gradual_insert_unordered_map(const std::vector<int>& fds) {
    std::unordered_map<int, SocketData> socket_map;

    auto start = std::chrono::high_resolution_clock::now();
    for (int fd : fds) {
        socket_map[fd] = {};
    }
    auto end = std::chrono::high_resolution_clock::now();
    return std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count() / (double)fds.size();
}

double benchmark_gradual_insert_simple_flat_map(const std::vector<int>& fds) {
    simple_flat_map<SocketData> socket_map(fds.size() * 2);

    auto start = std::chrono::high_resolution_clock::now();
    for (int fd : fds) {
        SocketData s;
        socket_map.insert(fd, std::move(s));
    }
    auto end = std::chrono::high_resolution_clock::now();
    return std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count() / (double)fds.size();
}

// Бенчмарки поиска с разными распределениями запросов
double benchmark_lookup_vector(const std::vector<int>& fds, int iterations, const std::vector<int>& lookup_fds) {
    VectorSocketStore store;
    for (int fd : fds) {
        store.insert(fd, SocketData{});
    }

    std::vector<double> times;
    for (int i = 0; i < iterations; ++i) {
        int fd = lookup_fds[i % lookup_fds.size()];
        auto start = std::chrono::high_resolution_clock::now();

        auto* data = store.find(fd);
        if (data) {
            data->data1++;
        }

        auto end = std::chrono::high_resolution_clock::now();
        times.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count());
    }
    return median(times);
}

double benchmark_lookup_sorted_vector(const std::vector<int>& fds, int iterations, const std::vector<int>& lookup_fds) {
    SortedVectorSocketStore store;
    for (int fd : fds) {
        store.insert(fd, SocketData{});
    }

    std::vector<double> times;
    for (int i = 0; i < iterations; ++i) {
        int fd = lookup_fds[i % lookup_fds.size()];
        auto start = std::chrono::high_resolution_clock::now();

        auto* data = store.find(fd);
        if (data) {
            data->data1++;
        }

        auto end = std::chrono::high_resolution_clock::now();
        times.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count());
    }
    return median(times);
}

double benchmark_lookup_unordered_map(const std::vector<int>& fds, int iterations, const std::vector<int>& lookup_fds) {
    std::unordered_map<int, SocketData> socket_map;
    for (int fd : fds) {
        socket_map[fd] = {};
    }

    std::vector<double> times;
    for (int i = 0; i < iterations; ++i) {
        int fd = lookup_fds[i % lookup_fds.size()];
        auto start = std::chrono::high_resolution_clock::now();

        auto it = socket_map.find(fd);
        if (it != socket_map.end()) {
            it->second.data1++;
        }

        auto end = std::chrono::high_resolution_clock::now();
        times.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count());
    }
    return median(times);
}

double benchmark_lookup_simple_flat_map(const std::vector<int>& fds, int iterations, const std::vector<int>& lookup_fds) {
    simple_flat_map<SocketData> socket_map(fds.size() * 2);
    for (int fd : fds) {
        socket_map.insert(fd, {});
    }

    std::vector<double> times;
    for (int i = 0; i < iterations; ++i) {
        int fd = lookup_fds[i % lookup_fds.size()];
        auto start = std::chrono::high_resolution_clock::now();

        auto* data = socket_map.find(fd);
        if (data) {
            data->data1++;
        }

        auto end = std::chrono::high_resolution_clock::now();
        times.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count());
    }
    return median(times);
}

#include <chrono>
#include <mutex>

class SnowflakeIDGenerator {
private:
    using TimePoint = std::chrono::time_point<std::chrono::system_clock, std::chrono::milliseconds>;
    // 41 бит для Timestamp (сдвиг 22)
    // 10 бит для Worker ID (сдвиг 12)
    // 12 бит для Sequence (0)
    static constexpr uint64_t WORKER_ID = 1; // Уникальный ID вашего узла
    static constexpr uint64_t CUSTOM_EPOCH = 1672531200000; // 01.01.2023 00:00:00 UTC

    uint64_t last_timestamp_ = 0;
    uint64_t sequence_ = 0;
    std::mutex mtx_;

    uint64_t get_current_timestamp() const {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::system_clock::now().time_since_epoch()
                   ).count() - CUSTOM_EPOCH;
    }

public:
    uint64_t next_id() {
        std::lock_guard<std::mutex> lock(mtx_);

        uint64_t timestamp = get_current_timestamp();

        if (timestamp < last_timestamp_) {
            // Ошибка: часы движутся назад
            throw std::runtime_error("Clock moved backwards.");
        }

        if (timestamp == last_timestamp_) {
            sequence_ = (sequence_ + 1) & 0xFFF; // 12 бит маска
            if (sequence_ == 0) {
                // Ждем до следующей миллисекунды
                while (timestamp == last_timestamp_) {
                    timestamp = get_current_timestamp();
                }
            }
        } else {
            sequence_ = 0;
        }

        last_timestamp_ = timestamp;

        // Компоновка ID: [Timestamp: 41] [Worker ID: 10] [Sequence: 12]
        return (timestamp << 22) | (WORKER_ID << 12) | sequence_;
    }
};

// Контейнер для имитации Socket Data
struct SocketDataWithID {
    uint64_t id;
    int data1 = 0;
    // ... другие данные
};
#include <memory>
class DoubleBufferMap {
private:
    using SocketID = uint64_t;
    using MapType = std::unordered_map<SocketID, SocketDataWithID>;

    // Два буфера
    std::unique_ptr<MapType> buffers_[2];
    std::atomic<int> current_read_buffer_ = 0; // Индекс активного буфера для чтения
    std::mutex write_mtx_; // Мьютекс для блокировки записи/обмена

public:
    DoubleBufferMap() {
        buffers_[0] = std::make_unique<MapType>();
        buffers_[1] = std::make_unique<MapType>();
    }

    // Поиск (Чтение) - без блокировки
    const SocketDataWithID* find(SocketID id) const {
        // Читаем индекс активного буфера
        const MapType& active_map = *buffers_[current_read_buffer_.load(std::memory_order_acquire)];

        auto it = active_map.find(id);
        if (it != active_map.end()) {
            return &it->second;
        }
        return nullptr;
    }

    // Обновление (Запись) - блокируется мьютексом
    void update(const MapType& new_map) {
        std::lock_guard<std::mutex> lock(write_mtx_);

        // Определяем неактивный буфер (тот, в который сейчас не читают)
        int write_index = 1 - current_read_buffer_.load(std::memory_order_relaxed);

        // 1. Копируем/строим новую карту в неактивный буфер
        *buffers_[write_index] = new_map;

        // 2. Атомарно меняем указатель, чтобы новый буфер стал активным для чтения
        current_read_buffer_.store(write_index, std::memory_order_release);
    }
};

// Замените ваши текущие реализации на эти, или добавьте их
// ВАЖНО: Мы используем uint64_t в качестве ключа

// std::unordered_map<uint64_t, SocketDataWithID>
double benchmark_lookup_uint64_unordered_map(const std::vector<uint64_t>& ids, int iterations, const std::vector<uint64_t>& lookup_ids) {
    std::unordered_map<uint64_t, SocketDataWithID> map;
    for (uint64_t id : ids) {
        map.emplace(id, SocketDataWithID{id});
    }

    std::vector<double> times;
    for (int i = 0; i < iterations; ++i) {
        uint64_t id = lookup_ids[i % lookup_ids.size()];
        auto start = std::chrono::high_resolution_clock::now();

        auto it = map.find(id);
        if (it != map.end()) {
            it->second.data1++;
        }

        auto end = std::chrono::high_resolution_clock::now();
        times.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count());
    }
    return median(times);
}

// flat_map<uint64_t, SocketDataWithID> (требует вашей реализации flat_map для uint64_t)
// Предположим, что simple_flat_map теперь принимает uint64_t
double benchmark_lookup_uint64_flat_map(const std::vector<uint64_t>& ids, int iterations, const std::vector<uint64_t>& lookup_ids) {
    // Используйте вашу реализацию flat_map, убедившись, что она поддерживает uint64_t
    simple_flat_map<SocketDataWithID> map(ids.size() * 2);
    for (uint64_t id : ids) {
        map.insert(id, SocketDataWithID{id});
    }

    std::vector<double> times;
    for (int i = 0; i < iterations; ++i) {
        uint64_t id = lookup_ids[i % lookup_ids.size()];
        auto start = std::chrono::high_resolution_clock::now();

        auto* data = map.find(id);
        if (data) {
            data->data1++;
        }

        auto end = std::chrono::high_resolution_clock::now();
        times.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count());
    }
    return median(times);
}

// Double Buffer Lookup (Чтение)
double benchmark_lookup_double_buffer(const std::vector<uint64_t>& ids, int iterations, const std::vector<uint64_t>& lookup_ids) {
    DoubleBufferMap db_map;
    std::unordered_map<uint64_t, SocketDataWithID> initial_map;

    for (uint64_t id : ids) {
        initial_map.emplace(id, SocketDataWithID{id});
    }
    db_map.update(initial_map); // Инициализация буфера

    std::vector<double> times;
    for (int i = 0; i < iterations; ++i) {
        uint64_t id = lookup_ids[i % lookup_ids.size()];
        auto start = std::chrono::high_resolution_clock::now();

        const auto* data = db_map.find(id); // O(1) чтение без блокировки
        if (data) {
            // Примечание: Мы не можем безопасно изменить data1 без блокировки,
            // поэтому просто имитируем успешный доступ.
            // (int)data->data1;
        }

        auto end = std::chrono::high_resolution_clock::now();
        times.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count());
    }
    return median(times);
}

// Функции генерации ID
std::vector<uint64_t> generate_snowflake_ids(int count) {
    SnowflakeIDGenerator generator;
    std::vector<uint64_t> ids;
    for (int i = 0; i < count; ++i) {
        ids.push_back(generator.next_id());
    }
    return ids;
}

std::vector<uint64_t> generate_random_uint64(int count) {
    std::vector<uint64_t> ids;
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<uint64_t> dis;
    for (int i = 0; i < count; ++i) {
        ids.push_back(dis(gen));
    }
    return ids;
}

#include <cstdint>
#include <vector>
#include <random>
#include <algorithm>

/**
 * @brief Генерирует вектор ID для операций поиска.
 * * Вектор состоит из 80% существующих ID (выбранных случайным образом)
 * и 20% совершенно случайных (несуществующих) ID.
 * * @param existing_ids Вектор ID, которые гарантированно есть в контейнере.
 * @param iterations Общее количество операций поиска.
 * @return std::vector<uint64_t> Вектор ID для бенчмарка.
 */
std::vector<uint64_t> generate_lookup_vector(
    const std::vector<uint64_t>& existing_ids,
    int iterations)
{
    if (existing_ids.empty()) {
        return {};
    }

    std::vector<uint64_t> lookup_ids;
    lookup_ids.reserve(iterations);

    std::random_device rd;
    std::mt19937_64 gen(rd());

    // Для выбора одного из существующих ID
    std::uniform_int_distribution<> existing_dis(0, existing_ids.size() - 1);

    // Для генерации совершенно случайного (несуществующего) ID
    std::uniform_int_distribution<uint64_t> random_dis;

    for (int i = 0; i < iterations; ++i) {
        // Логика 80% существующие / 20% случайные (несуществующие)
        if (gen() % 100 < 80) {
            // 80%: выбираем существующий ID
            lookup_ids.push_back(existing_ids[existing_dis(gen)]);
        } else {
            // 20%: генерируем случайный uint64_t (высока вероятность несуществующего)
            lookup_ids.push_back(random_dis(gen));
        }
    }

    // Перемешиваем вектор, чтобы избежать артефактов, если generate_snowflake_ids
    // создал последовательные ID.
    std::shuffle(lookup_ids.begin(), lookup_ids.end(), gen);

    return lookup_ids;
}

int main() {
    std::vector<int> sizes = {500, 1000, 5000, 10'000, 100'000};
    int iterations = 1000'000;

    // std::cout << "=== Realistic Socket FD Benchmark (gradual accept scenario) ===\n";
    // std::cout << "Size\tSeq\tInsertVec\tInsertSVec\tInsertUM\tInsertSFM\tLookupVec\tLookupSVec\tLookupUM\tLookupSFM\n";
    // std::cout << "----\t---\t---------\t----------\t--------\t---------\t---------\t----------\t--------\t---------\n";

    // for (int size : sizes) {
    //     for (bool sequential : {true, false}) {
    //         auto fds = sequential ? generate_sequential_fds(size) : generate_random_fds(size);

    //         // Генерируем FD для поиска - 80% существующих, 20% случайных
    //         std::vector<int> lookup_fds;
    //         std::random_device rd;
    //         std::mt19937 gen(rd());
    //         std::uniform_int_distribution<> existing_dis(0, fds.size() - 1);
    //         std::uniform_int_distribution<> random_dis(100, 1000000);

    //         for (int i = 0; i < iterations; ++i) {
    //             if (gen() % 100 < 80) { // 80% существующих FD
    //                 lookup_fds.push_back(fds[existing_dis(gen)]);
    //             } else { // 20% случайных (несуществующих) FD
    //                 lookup_fds.push_back(random_dis(gen));
    //             }
    //         }

    //         double iv = benchmark_gradual_insert_vector(fds);
    //         double isv = benchmark_gradual_insert_sorted_vector(fds);
    //         double ium = benchmark_gradual_insert_unordered_map(fds);
    //         double isfm = benchmark_gradual_insert_simple_flat_map(fds);

    //         double lv = benchmark_lookup_vector(fds, iterations, lookup_fds);
    //         double lsv = benchmark_lookup_sorted_vector(fds, iterations, lookup_fds);
    //         double lum = benchmark_lookup_unordered_map(fds, iterations, lookup_fds);
    //         double lsfm = benchmark_lookup_simple_flat_map(fds, iterations, lookup_fds);

    //         std::cout << size << "\t"
    //                   << (sequential ? "Y" : "N") << "\t"
    //                   << iv << "\t\t"
    //                   << isv << "\t\t"
    //                   << ium << "\t\t"
    //                   << isfm << "\t\t"
    //                   << lv << "\t\t"
    //                   << lsv << "\t\t"
    //                   << lum << "\t\t"
    //                   << lsfm << "\n";
    //     }


    // }



    std::cout << "\n=== uint64_t ID Benchmark (Snowflake/Random) ===\n";
    std::cout << "Size\tType\t\tLookupUM\tLookupSFM\tLookupDB (UM)\n";
    std::cout << "----\t----\t\t--------\t---------\t-----------\n";

    for (int size : sizes) {
        // 1. Snowflake IDs (упорядоченные)
        auto snowflake_ids = generate_snowflake_ids(size);
        std::vector<uint64_t> lookup_sf_ids = generate_lookup_vector(snowflake_ids, iterations);

        double lum_sf = benchmark_lookup_uint64_unordered_map(snowflake_ids, iterations, lookup_sf_ids);
        double lsfm_sf = benchmark_lookup_uint64_flat_map(snowflake_ids, iterations, lookup_sf_ids);
        double ldb_sf = benchmark_lookup_double_buffer(snowflake_ids, iterations, lookup_sf_ids);

        std::cout << size << "\tSnowflake\t"
                  << lum_sf << "\t\t"
                  << lsfm_sf << "\t\t"
                  << ldb_sf << "\n";

        // 2. Random IDs (случайные)
        auto random_ids = generate_random_uint64(size);
        std::vector<uint64_t> lookup_rand_ids = generate_lookup_vector(random_ids, iterations);

        double lum_rand = benchmark_lookup_uint64_unordered_map(random_ids, iterations, lookup_rand_ids);
        double lsfm_rand = benchmark_lookup_uint64_flat_map(random_ids, iterations, lookup_rand_ids);
        double ldb_rand = benchmark_lookup_double_buffer(random_ids, iterations, lookup_rand_ids);

        std::cout << size << "\tRandom\t\t"
                  << lum_rand << "\t\t"
                  << lsfm_rand << "\t\t"
                  << ldb_rand << "\n";
    }


    return 0;
}
