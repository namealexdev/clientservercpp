#ifndef S_H
#define S_H

#include <cstdint>
#include <unordered_map>
#include <memory>
#include <stdexcept>

template<typename Value>
class DualUnorderedMapRegistry {
public:
    using SocketID = uint64_t;
    using SocketFD = int;
    using SocketData = Value;

private:
    // O(1) Lookup: Поиск данных по ID (для маршрутизации пакетов по ID)
    std::unordered_map<SocketID, SocketData> id_to_data_;

    // O(1) Lookup: Поиск ID по FD (самый эффективный для цикла epoll/kqueue)
    std::unordered_map<SocketFD, SocketData> fd_to_data_;

    // пока достаточно, можно потом переделать на snowflake
    uint64_t next_id_counter_ = 1;

public:
    void insert(SocketFD fd, const SocketData& val) {
        // Проверка на повторное использование FD: если FD уже есть, удаляем старый
        if (fd_to_data_.count(fd)) {
            // Удаляем старый сокет по его старому ID, прежде чем переписывать.
            SocketID old_id = fd_to_data_[fd]->id;
            id_to_data_.erase(old_id);
            // fd_to_data_[fd] будет перезаписан ниже
        }

        SocketID new_id = next_id_counter_++;
        // auto new_data = std::make_shared<SocketData>();
        // new_data->id = new_id;
        // new_data->file_descriptor = fd;

        // Обновление структур
        fd_to_data_[fd] = val; // O(1)
        id_to_data_[new_id] = val; // O(1)
    }

    void del(SocketFD fd) {
        auto it_fd = fd_to_data_.find(fd);
        if (it_fd != fd_to_data_.end()) {
            SocketID id_to_remove = it_fd->second->id;

            // Удаляем из карты ID
            id_to_data_.erase(id_to_remove); // O(1)

            // Удаляем из карты FD
            fd_to_data_.erase(it_fd); // O(1)
        }
    }

    // Самый эффективный поиск по FD (O(1) в среднем)
    SocketData* find_by_fd(SocketFD fd) const {
        auto it = fd_to_data_.find(fd);
        if (it != fd_to_data_.end()) {
            return it->second;
        }
        return nullptr;
    }

    // Поиск по ID (O(1) в среднем)
    SocketData find_by_id(SocketID id) const {
        auto it = id_to_data_.find(id);
        if (it != id_to_data_.end()) {
            return it->second;
        }
        return nullptr;
    }
};

#endif // S_H
