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
struct DataReceived{
    int size;
    char* data;
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

// общий простой епол без сокетов - прокидывает события
class BaseEpoll {
public:
    BaseEpoll();
    virtual ~BaseEpoll();

    bool AddFd(int fd);
    void RemoveFd(int fd);
    void RunEpoll();
    void StopEpoll();

    // прокидывает все вызовы сюда
    // void (*on_event_handlers)(int fd, uint32_t events) = 0;

    // std::function<void(int fd, uint32_t events)> onEvent = 0;
    void SetOnReadAcceptHandler(std::function<void(int)> handler) { on_read_ = std::move(handler); };
    void SetOnWriteHandler(std::function<void(int)> handler) { on_write_ = std::move(handler); };
    // void SetErrorHandler(std::function<void(int)> handler) { on_error_ = std::move(handler); };
    void SetDisconnectHandler(std::function<void(int)> handler) { on_hangup_ = std::move(handler); };

private:
    int epfd_ = -1;

    std::function<void(int)> on_read_;
    std::function<void(int)> on_write_;
    // std::function<void(int)> on_error_;
    std::function<void(int)> on_hangup_;

    void ExecLoop();// блокирует
    std::unique_ptr<std::thread> thLoop_ = 0;
    bool need_stop_ = false;
};




#endif // EPOLL_H
