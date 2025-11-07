#ifndef SERVER_H
#define SERVER_H

#include "const.h"
#include "epoll.h"
#include "stats.h"

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
    virtual bool start(int num_workers = 0) = 0; // wait accept
    virtual void stop() = 0;
    virtual int countClients() = 0;

    ServerConfig conf_;
    string last_error_;
    Stats stats_;

    string getServerState();

protected:
    ServerState state_ = ServerState::STOPPED;
    int create_listen_socket();
};


class SimpleServer : public IServer{
public:
    SimpleServer(ServerConfig config, EventDispatcher* e = nullptr);
    bool start(int num_workers = 0);
    void stop();
    int countClients();

    // нужен для передачи сокетов в многопоточном сервере
    bool addClientFd(int fd, const Stats& st);

    void addHandlerEvent(EventType type, std::function<void(void*)> handler);

private:
    void onEpollEvent(int fd, uint32_t events);
    void handleAccept(); // принимает клиентов
    void handleClientData(int fd); // обрабатывает данные от клиента
    void removeClient(int fd); // удаляет клиента из epoll и списка

    int listen_socket_ = -1;
    EventDispatcher* dispatcher_ = 0;
    BaseEpoll epoll_;
    static constexpr size_t BUF_SIZE = 65536;
    char buffer[BUF_SIZE];

    std::unordered_map<int, Stats> clients_;

    // WARN: нужно только для мультипотока
    std::queue<std::pair<int, Stats>> preClient_socks_;
    std::mutex preClient_socks_mtx_;
};

class MultithreadServer : public IServer{
public:
    MultithreadServer(ServerConfig config);

    bool start(int num_workers);
    void stop();
    int countClients();

    void addHandlerEvent(EventType type, std::function<void(void*)> handler);

private:
    void onEpollEvent(int fd, uint32_t events); // только для accept
    void handleAccept(); // распределяет сокеты по воркерам

    int listen_socket_ = -1;
    EventDispatcher* dispatcher_ = 0;
    BaseEpoll accept_epoll_; // только для accept

    std::vector<std::unique_ptr<SimpleServer>> workers_;
    std::vector<int> worker_client_counts_; // для балансировки(чтобы без atomic)
};


// class SinglethreadServer : public IServer, public IClientEventHandler {
// public:
//     SinglethreadServer(ServerConfig&& conf) :
//         IServer(std::move(conf)), epoll_(this){

//     }
//     bool start();
//     void stop();
//     int countClients();

// private:
//     void onEvent(EventType e);

//     ServerLightEpoll epoll_;
// };

// class MultithreadServer : public IServer, public IClientEventHandler {
// public:
//     MultithreadServer(ServerConfig&& conf);
//     bool start(int count_ths);
//     void stop();

//     int countClients();
// private:
//     ServerMultithEpoll epoll_;
// };



#endif // SERVER_H
