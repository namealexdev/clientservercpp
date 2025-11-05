#ifndef EPOLL_H
#define EPOLL_H

#include <functional>
#include <sys/epoll.h>
#include "const.h"
#include "stats.h"



// общий простой епол без сокетов

class IEpoll {
protected:
    IEpoll();
    ~IEpoll();

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
//
class ClientLightEpoll : protected IEpoll
{
public:
    // using HandlerPtr = void (LightEpoll::*)(int, uint32_t);
    // HandlerPtr handler_ptr = &LightEpoll::event_handlers;
    // (this->*handler_ptr)(fd, evs);

    ClientLightEpoll();
    void start_handle(int sock);
    void stop();

    bool need_reconnect_ = false;
    std::function<void(const char* data, ssize_t size)> on_recv_handler = 0;

    // now in socket
    void send(char* d, int sz);
    // using queue or lockfree queue
    void queue_add(char* d, int sz);
    // get from q and call send
    void queue_send(char* d, int sz);


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
    ServerLightEpoll();

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

};

// wait th + n th recv clns
// добавляем распределение по потокам
class ServerMultithEpoll : protected IEpoll
{
public:
    ServerMultithEpoll();

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

// Многопоточный epoll
// class MultiThreadEpoll {
// public:
//     MultiThreadEpoll();
//     ~MultiThreadEpoll();

//     bool init();
//     bool add_fd(int fd, uint32_t events);
//     bool remove_fd(int fd);
//     void run();

//     // Колбеки - вызываются в worker threads
//     std::function<void(int fd, uint32_t events)> on_event;

// private:
//     // Внутренняя многопоточная реализация
//     class ThreadPool {
//         // твоя многопоточная логика из MainEpoll
//     };

//     std::unique_ptr<ThreadPool> thread_pool_;
// };











// class ClientEpoll : public Epoll{
//     ClientEpoll(){
//     }
//     // клиенты
// };

// class ServerEpoll : public Epoll{
// };

#endif // EPOLL_H
