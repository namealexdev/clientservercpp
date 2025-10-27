#ifndef FM_H
#define FM_H

#include <cstdint>
#include <vector>
#include <algorithm> // Для бинарного поиска в flat_map
// Предполагаем, что ваш "simple_flat_map" реализован как std::vector<std::pair<Key, Value>>
// и использует std::lower_bound для поиска.

// template<typename T>
// class SimpleFlatMap {
//     // ... ваша реализация, где ключом является int FD
//     // Мы предполагаем, что Key=int и Value=std::shared_ptr<SocketData>
// public:
//     using SocketFD = int;
//     using SocketDataPtr = std::shared_ptr<SocketData>;
// private:
//     std::vector<std::pair<SocketFD, SocketDataPtr>> data_;

// public:
//     // ... insert, erase и т.д. ...

//     // Поиск по FD (O(log N) с отличным кэшем)
//     SocketDataPtr find_by_fd(SocketFD fd) const {
//         auto it = std::lower_bound(
//             data_.begin(), data_.end(), fd,
//             [](const auto& pair, SocketFD val) { return pair.first < val; }
//             );

//         if (it != data_.end() && it->first == fd) {
//             return it->second;
//         }
//         return nullptr;
//     }

//     // ...
// };

// class FlatMapFDRegistry {
// public:
//     using SocketID = uint64_t;
//     using SocketFD = int;
//     using SocketDataPtr = std::shared_ptr<SocketData>;

// private:
//     // O(log N) Lookup: Поиск данных по FD
//     SimpleFlatMap<SocketDataPtr> fd_to_data_;

//     // O(1) Lookup: Поиск данных по ID (если нужно)
//     // Эта карта все равно необходима, если требуется поиск по ID!
//     std::unordered_map<SocketID, SocketDataPtr> id_to_data_ptr_;

//     uint64_t next_id_counter_ = 1;

// public:
//     void register_socket(SocketFD fd) {
//         // ... (логика вставки в SimpleFlatMap и обновления id_to_data_ptr_)
//     }

//     void unregister_socket(SocketFD fd) {
//         // ... (логика удаления из SimpleFlatMap и id_to_data_ptr_)
//     }

//     // Поиск по FD: Самый быстрый на вашем железе (O(log N))
//     SocketDataPtr find_by_fd(SocketFD fd) const {
//         return fd_to_data_.find_by_fd(fd);
//     }

//     // Поиск по ID: O(1) в среднем
//     SocketDataPtr find_by_id(SocketID id) const {
//         auto it = id_to_data_ptr_.find(id);
//         if (it != id_to_data_ptr_.end()) {
//             return it->second;
//         }
//         return nullptr;
//     }
// };

#endif // FM_H
