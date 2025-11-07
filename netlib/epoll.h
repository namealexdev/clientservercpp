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
    ClientConnect,
    ClientDisconnect,
    DataReceived
};

class EventDispatcher {
public:
    // call event
    void onEvent(EventType type, void* data = nullptr){
        if (auto it = handlers_.find(type); it != handlers_.end()) {
            it->second(data);
        }
    };
    // set event
    void setHandler(EventType type, std::function<void(void*)> handler){
        handlers_[type] = std::move(handler);
    };

private:
    std::unordered_map<EventType, std::function<void(void*)>> handlers_;
};

// class IClientEventHandler {
//     public:
//         virtual ~IClientEventHandler() = default;
//         virtual void onEvent(EventType e) = 0;
// };


// общий простой епол без сокетов
class BaseEpoll {
public:
    BaseEpoll();
    virtual ~BaseEpoll();

    bool need_stop_ = false;

    bool add_fd(int fd, uint32_t events);
    void remove_fd(int fd);
    void start();
    void stop();

    // прокидывает все вызовы сюда
    // void (*on_event_handlers)(int fd, uint32_t events) = 0;
    std::function<void(int fd, uint32_t events)> onEvent = 0;

private:
    int epfd_ = -1;
    static const int MAX_EVENTS = 64;
    static const int EPOLL_TIMEOUT = 100;

    void execLoop();// блокирует
    std::thread* thLoop_ = 0;
};




#endif // EPOLL_H
