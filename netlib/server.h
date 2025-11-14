#ifndef SERVER_H
#define SERVER_H

#include "const.h"
#include "epoll.h"

#include <cassert>
#include <condition_variable>
#include <mutex>

#include "libinclude/iserver.h"

/*
struct PacketParser {

    // union {
    //     uint32_t net_value; // Сетевой порядок (big-endian)
    //     char bytes[PACKET_HEADER_SIZE];// Читаем по байтам
    // } header;
    // uint8_t pos_header = 0; // Прогресс чтения

    // Данные пакета
    uint32_t payload_size = 0; // Размер в хостовом порядке
    // std::vector<char> payload;   // Буфер (переиспользуется)
    char data[PACKET_MAX_SIZE];
    uint32_t pos_data = 0;   // Прогресс чтения данных

    inline void Reset() {
        // pos_header = 0;
        payload_size = 0;
        // pos_payload = 0;
        pos_data = 0;
        // payload.resize(0);
    }

    // сохраняет данные пакета если они есть.
    // return кол-во оставшихся байт
    int ParseDataPacket(char* buf, int sz){
        int count_read = 0;
        // читаем 4 байта - size
        if (pos_data < PACKET_HEADER_SIZE) {
            int canread = std::min(sz, PACKET_HEADER_SIZE);
            memcpy(data, buf, canread);
            pos_data += canread;
            count_read += canread;

            // Ждём ещё
            if (pos_data < PACKET_HEADER_SIZE) return sz - count_read;

            payload_size = ntohl(*data); // Парсим размер

            // ВАЛИДАЦИЯ
            assert(payload_size <= PACKET_MAX_PAYLOAD_SIZE);
            // if (payload_size == 0 ) {
            //     std::cerr << "Invalid packet size: " << payload_size << " from fd " << fd << std::endl;
            // }
            // payload.resize(payload_size); // Выделяем/переиспользуем память
        }

        // читаем данные
        if (pos_data < payload_size) {
            int canread = std::min(payload_size, PACKET_MAX_PAYLOAD_SIZE);
            memcpy(data + PACKET_HEADER_SIZE, buf, canread);
            pos_data += canread;
            count_read += canread;
        }

        return sz - count_read;
    }

    inline bool IsPacketReady(){// правильно?
        return payload_size > 0 &&
               payload_size + PACKET_HEADER_SIZE == pos_data + 1;
    }
};*/

struct PacketParser {
    std::vector<char> data;           // Буфер для данных (заголовок + полезная нагрузка)
    uint32_t payload_size = 0;        // Размер полезной нагрузки в хостовом порядке
    uint32_t bytes_received = 0;      // Общее количество полученных байт
    bool header_parsed = false;       // Флаг, что заголовок распарсен

    inline void Reset() {
        data.clear();
        payload_size = 0;
        bytes_received = 0;
        header_parsed = false;
    }

    // Сохраняет данные пакета если они есть
    // Возвращает количество оставшихся байт
    int ParseDataPacket(const char* buf, int sz) {
        int count_read = 0;

        // Если заголовок еще не распарсен
        if (!header_parsed) {
            // Сколько байт нужно дочитать до полного заголовка
            int header_needed = PACKET_HEADER_SIZE - bytes_received;
            int can_read = std::min(sz, header_needed);

            // Добавляем данные в вектор
            data.insert(data.end(), buf, buf + can_read);
            count_read += can_read;
            bytes_received += can_read;

            // Если заголовок полностью получен
            if (bytes_received >= PACKET_HEADER_SIZE) {
                // Извлекаем размер полезной нагрузки
                uint32_t net_size;
                memcpy(&net_size, data.data(), sizeof(net_size));
                payload_size = ntohl(net_size);

                // Валидация
                // if (payload_size > PACKET_MAX_PAYLOAD_SIZE) {
                //     std::cerr << "Invalid packet size: " << payload_size << std::endl;
                //     Reset();
                //     return -1; // Ошибка
                // }

                header_parsed = true;

                data.resize(PACKET_HEADER_SIZE + payload_size);
            }
        }

        // Если заголовок распарсен и есть еще данные для чтения
        if (header_parsed && count_read < sz) {
            const char* payload_start = buf + count_read;
            int payload_remaining = payload_size - (bytes_received - PACKET_HEADER_SIZE);
            int can_read = std::min(sz - count_read, payload_remaining);

            // Добавляем полезную нагрузку
            data.insert(data.end(), payload_start, payload_start + can_read);
            count_read += can_read;
            bytes_received += can_read;
        }

        return sz - count_read;
    }

    inline bool IsPacketReady() const {
        return header_parsed &&
               (bytes_received == PACKET_HEADER_SIZE + payload_size);
    }

    // Вспомогательные методы для доступа к данным
    inline const char* GetPayloadData() const {
        return data.data() + PACKET_HEADER_SIZE;
    }

    inline uint32_t GetPayloadSize() const {
        return payload_size;
    }

    inline const std::vector<char>& GetFullPacket() const {
        return data;
    }
};

struct ClientData{
    // только для handshake?
    // enum ClientState {
    //     HANDSHAKE,
    //     DATA
    // } state;
    // std::array<uint8_t, 16> client_uuid;

    Stats stats;
    // очередь пакетов для клиента?
};



class SimpleServer : public IServer{
public:
    SimpleServer(ServerConfig config, EventDispatcher* e = nullptr);
    bool StartListen(int num_workers = 0);
    void StartWait();
    void Stop();
    int CountClients();

    // нужен для передачи сокетов в многопоточном сервере
    void AddClientFd(int fd, const Stats& st);

    void AddHandlerEvent(EventType type, std::function<void(void*)> handler);

private:
    void handleAccept(); // принимает клиентов
    void handleClientData(int fd); // обрабатывает данные от клиента
    void removeClient(int fd); // удаляет клиента из epoll и списка

    int listen_socket_ = -1;
    EventDispatcher* dispatcher_ = 0;
    BaseEpoll epoll_;
    char buffer_[BUF_READ_SIZE];

    std::unordered_map<int, ClientData> clients_;
    //for add client (from other thread when MultithreadServer) and erace
    std::mutex clients_mtx_;

    PacketParser pktReader_;
};

class MultithreadServer : public IServer{
public:
    explicit MultithreadServer(ServerConfig config);

    bool StartListen(int num_workers = 1);
    void Stop();
    int CountClients();

    void AddHandlerEvent(EventType type, std::function<void(void*)> handler);

private:
    void handleAccept(int fd); // распределяет сокеты по воркерам

    int listen_socket_ = -1;
    EventDispatcher* dispatcher_ = 0;
    BaseEpoll accept_epoll_; // только для accept

    std::vector<std::unique_ptr<SimpleServer>> workers_;
    std::vector<int> worker_client_counts_; // для балансировки(чтобы без atomic)
};

#endif // SERVER_H
