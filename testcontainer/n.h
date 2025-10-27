#ifndef N_H
#define N_H

#include <cstdint>
#include <vector>
#include <unordered_map>
#include <memory>
#include <stdexcept>

// 1. Структура данных сокета
struct SocketData {
    uint64_t id;      // Ваш счетчик uint64_t
    int file_descriptor;
    // ... другие данные
};

// 2. Индексированный реестр сокетов
class FDSocketRegistry {
public:
    using SocketID = uint64_t;
    using SocketFD = int;
    using SocketDataPtr = std::shared_ptr<SocketData>;

private:
    // O(1) Lookup: Вектор, индексированный по FD
    // Хранит указатели на данные сокетов. NULL означает, что FD не используется.
    std::vector<SocketDataPtr> fd_to_data_;

    // O(1) Lookup: Хэш-таблица для обратного поиска по ID
    // Необходима, если вам нужно найти сокет по ID.
    // Если вам НЕ нужен поиск по ID, эту map можно удалить.
    std::unordered_map<SocketID, SocketDataPtr> id_to_data_ptr_;

    uint64_t next_id_counter_ = 1;

public:
    /**
     * @brief Регистрирует новый сокет, генерируя новый ID.
     */
    void register_socket(SocketFD fd) {
        if (fd < 0) return;

        // 1. Проверка размера вектора: расширяем, если FD выходит за пределы
        if (fd >= fd_to_data_.size()) {
            fd_to_data_.resize(fd + 1, nullptr);
        }

        // 2. Обработка повторного использования FD (самое важное)
        // Если старый сокет по этому FD еще есть (ошибка логики или задержка)
        if (fd_to_data_[fd] != nullptr) {
            // Удаляем старый сокет из карты по его старому ID, прежде чем переписывать.
            // (В реальной жизни лучше сначала явно unregister_socket вызвать)
            id_to_data_ptr_.erase(fd_to_data_[fd]->id);
        }

        // 3. Создаем новый объект данных
        SocketID new_id = next_id_counter_++;
        auto new_data = std::make_shared<SocketData>();
        new_data->id = new_id;
        new_data->file_descriptor = fd;

        // 4. Обновляем структуры данных
        fd_to_data_[fd] = new_data;
        id_to_data_ptr_[new_id] = new_data; // Добавляем для поиска по ID
    }

    /**
     * @brief Удаляет сокет по его FD.
     */
    void unregister_socket(SocketFD fd) {
        if (fd < 0  fd >= fd_to_data_.size()  fd_to_data_[fd] == nullptr) {
            return; // Сокет не найден
        }

        SocketID id_to_remove = fd_to_data_[fd]->id;

        // Удаляем из карты ID
        id_to_data_ptr_.erase(id_to_remove);

        // Удаляем из вектора FD (устанавливаем nullptr)
        fd_to_data_[fd] = nullptr;

        // Примечание: std::shared_ptr теперь освободит память, когда счетчик ссылок станет 0.
    }

    /**
     * @brief Самый эффективный поиск: по FD (O(1)).
     */
    SocketDataPtr find_by_fd(SocketFD fd) const {
        if (fd >= 0 && fd < fd_to_data_.size()) {
            return fd_to_data_[fd]; // O(1) доступ
        }
        return nullptr;
    }

    /**
     * @brief Поиск по ID (O(1) в среднем).
     */
    SocketDataPtr find_by_id(SocketID id) const {
        auto it = id_to_data_ptr_.find(id);
        if (it != id_to_data_ptr_.end()) {
            return it->second;
        }
        return nullptr;
    }
};

#endif // N_H
