#ifndef SERVER_H
#define SERVER_H

#include "const.h"
#include "epoll.h"

#include <cassert>
#include <condition_variable>
#include <mutex>

#include "libinclude/iserver.h"

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
};

struct ClientData{
    // только для handshake?
    enum ClientState {
        HANDSHAKE,
        DATA
    } state;
    std::array<uint8_t, 16> client_uuid;

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
