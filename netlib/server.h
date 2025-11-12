#ifndef SERVER_H
#define SERVER_H

#include "const.h"
#include "epoll.h"

#include <condition_variable>
#include <mutex>

#include "libinclude/iserver.h"

struct ClientBuffer {
    #define HEADER_SIZE 4
    #define MAX_PAYLOAD_SIZE 16 * 1024 * 1024 // 16MB защита
    
    union {
        uint32_t net_value; // Сетевой порядок (big-endian)
        char bytes[HEADER_SIZE];// Читаем по байтам
    } header;
    uint8_t pos_header = 0; // Прогресс чтения

    // Данные пакета
    uint32_t payload_size = 0; // Размер в хостовом порядке
    std::vector<char> payload;   // Буфер (переиспользуется)
    uint32_t pos_payload = 0;   // Прогресс чтения данных

    void reset() {
        pos_header = 0;
        payload_size = 0;
        pos_payload = 0;
        payload.resize(0);
    }
};

struct ClientData{
    // только для handshake?
    enum ClientState {
        HANDSHAKE,
        DATA
    } state;
    std::array<uint8_t, 16> client_uuid;

    ClientBuffer buf;
    Stats stats;
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
    char buffer[BUF_SIZE];

    std::unordered_map<int, ClientData> clients_;

    // WARN: нужно только для мультипотока
    // std::queue<std::pair<int, Stats>> preClient_socks_;
    std::mutex preClient_socks_mtx_;
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
