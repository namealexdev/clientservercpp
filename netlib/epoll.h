#ifndef EPOLL_H
#define EPOLL_H

#include <functional>
#include <sys/epoll.h>
#include <atomic>
#include "const.h"
#include "stats.h"


enum class EventType {
    Disconnected,
    Reconnected,
    Waiting,

    ClientDisconnect
};


class IClientEventHandler {
public:
    virtual ~IClientEventHandler() = default;
    virtual void onEvent(EventType e) = 0;
};


// общий простой епол без сокетов

class IEpoll {
protected:
    IEpoll();
    ~IEpoll();

    IClientEventHandler* clientHandler_ = nullptr;

    // template<typename Derived>
    void exec();// блокирует
    // прокидывает все вызовы сюда
    // void (*on_event_handlers)(int fd, uint32_t events) = 0;
    std::function<void(int fd, uint32_t events)> on_event_handlers = 0;

    bool add_fd(int fd, uint32_t events);
    void remove_fd(int fd);

    bool need_stop_ = false;

private:
    int epfd_ = -1;
    static const int MAX_EVENTS = 64;
};

// for client
class ClientLightEpoll : protected IEpoll
{
public:
    // using HandlerPtr = void (LightEpoll::*)(int, uint32_t);
    // HandlerPtr handler_ptr = &LightEpoll::event_handlers;
    // (this->*handler_ptr)(fd, evs);

    ClientLightEpoll(IClientEventHandler* clh);
    void start_handle(int sock);
    void stop();

    bool need_reconnect_ = false;
    // std::function<void(const char* data, ssize_t size)> on_recv_handler = 0;

    //ВЫНЕСТИ ЭТО в класс для send recv
    // now in socket
    void send(char* d, int sz);
    // using queue or lockfree queue
    void queue_add(char* d, int sz);
    // get from q and call send
    void queue_send();


private:
    void on_epoll_event(int fd, uint32_t evs);
    void handle_socket_data();

    std::thread* handleth_ = 0;
    static constexpr size_t BUF_SIZE = 65536;
    char buffer[BUF_SIZE];
    int socket_ = -1;

    std::queue<std::pair<char*,int>> queue_;
};

// должен быть тем же что и ClientLightEpoll
// но для сервера, то есть делать accept
// то что добавили: countClients, clients handle_accept
class ServerLightEpoll : protected IEpoll
{
public:
    ServerLightEpoll(IClientEventHandler* clh);

    void start_handle(int sock);
    void stop();
    int countClients();

private:
    void on_epoll_event(int fd, uint32_t evs);

    void remove_client(int fd);
    void handle_client_data(int fd);
    void handle_accept();

    std::thread* handleth_ = 0;
    int socket_ = -1;
    static constexpr size_t BUF_SIZE = 65536;
    char buffer[BUF_SIZE];

    std::unordered_map<int, Stats> clients;
};


// wait th + queue send th
// добавляем асинхронную очередь пакетов
class ClientMultithEpoll : protected IEpoll
{
public:
    ClientMultithEpoll(IClientEventHandler* clh);
    void start_handle(int sock);
    void stop();

    bool need_reconnect_ = false;

    //ВЫНЕСТИ ЭТО в класс для send recv
    // now in socket
    void send(char* d, int sz);
    // using queue or lockfree queue
    void queue_add(char* d, int sz);
    // get from q and call send
    void queue_send();

private:
    void on_epoll_event(int fd, uint32_t evs);
    void handle_socket_data();

    std::thread* handleth_ = 0;
    static constexpr size_t BUF_SIZE = 65536;
    char buffer[BUF_SIZE];
    int socket_ = -1;

    void start_queue();
    std::thread* queue_th_;
    std::mutex mtx_queue_;
    std::queue<std::pair<char*,int>> queue_;
};

// wait th + n th recv clns
// добавляем распределение по потокам
class ServerSubEpoll : protected IEpoll
{
public:
    ServerSubEpoll();
    void start_handle(int sock);
    void stop();
    int countClients();

    // очередь для передачи сокетов между потоками
    void push_external_socket(int client_fd, const Stats &st);

private:
    void on_epoll_event(int fd, uint32_t evs);

    void remove_client(int fd);

    void handle_client_data(int fd);

    std::thread* handleth_ = 0;
    int socket_ = -1;
    static constexpr size_t BUF_SIZE = 65536;
    char buffer[BUF_SIZE];
    std::unordered_map<int, Stats> clients;


    // int wakeup_fd = -1; // для пробуждения epoll
    std::queue<std::pair<int, Stats>> pending_new_socks_; // новые сокеты от MainEpoll
    std::mutex mtx_pending_new_socks_; // защищает очередь
};


// отличия от ServerLightEpoll что есть subepolls_ и accept_handler будет сразу балансить
// тут тред будет только на accept
class ServerMultithEpoll : protected IEpoll
{
public:
    ServerMultithEpoll(IClientEventHandler* clh);
    ~ServerMultithEpoll();

    void start_handle(int sock, int count_workers);
    void stop();
    int countClients();

private:
    void on_epoll_event(int fd, uint32_t evs);
    void handle_accept();
    std::vector<ServerSubEpoll*> subepolls_;

    std::thread* handleth_ = 0;
    int socket_ = -1;
};



#endif // EPOLL_H
