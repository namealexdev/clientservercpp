#ifndef SERVER_H
#define SERVER_H

#include "const.h"
#include "epoll.h"
#include "stats.h"


struct ClientBuffer {
    // Заголовок: 4 байта размера в сетевом порядке
    union {
        uint32_t net_value; // Используем после полного чтения
        char bytes[4];      // Читаем по байтам
    } header;
    uint8_t header_read = 0; // Прогресс чтения заголовка [0..4]

    // Данные пакета
    uint32_t payload_size = 0; // Размер в хостовом порядке
    std::vector<char> payload;   // Буфер (переиспользуется)
    uint32_t payload_read = 0;   // Прогресс чтения данных

    // Подготовка к следующему пакету (быстро, без освобождения памяти)
    void reset() {
        header_read = 0;
        payload_size = 0;
        payload_read = 0;
        payload.resize(0); // Только изменяет size, capacity остаётся
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


struct ServerConfig{
    string host = "0.0.0.0";
    int port = 12345;
    // string filename;// empty = without write

    // int recv_buffer_size = 1 * 1024 * 1024; // 100 MiB
    int max_connections = 10;

    // int serialization_ths = 1;
};

enum class ServerState : uint8_t{
    STOPPED, // default
    WAITING,
    ERROR,
};

class IServer{
public:
    IServer(ServerConfig&& c) : conf_(std::move(c)) {};
    virtual bool StartListen(int num_workers = 0) = 0; // wait accept
    virtual void Stop() = 0;
    virtual int CountClients() = 0;
    virtual void AddHandlerEvent(EventType type, std::function<void(void*)> handler) = 0;

    Stats& GetStats(){return stats_;}
    std::string_view GetLastError(){return last_error_;}
    string GetServerState();

protected:
    ServerState state_ = ServerState::STOPPED;
    int create_listen_socket();

    ServerConfig conf_;
    string last_error_;
    Stats stats_;
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
