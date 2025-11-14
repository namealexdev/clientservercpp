#ifndef EPOLL_H
#define EPOLL_H

#include <functional>
#include <sys/epoll.h>
#include <atomic>
#include "const.h"

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

#pragma pack(push, 1)
// ждем от клиента 20 байт
struct ClientHiMsg{
    std::array<uint8_t, 16> uuid;
};
// ждем от сервера ответ 24 байта
struct ServerAnsHiMsg{
    enum ClientMode : uint8_t{
        ERRUUID,
        SEND
    };
    // подтверждение что точно наш сервер
    std::array<uint8_t, 16> client_uuid;
    // что клиенту делать дальше
    ClientMode client_mode;
};
static_assert(sizeof(ClientHiMsg) == 16, "Size mismatch");
static_assert(sizeof(ServerAnsHiMsg) == 17, "Size mismatch");
#pragma pack(pop)


/*
 * клиент отправляет uuid
 * ждем тип (что клиенту делать)
 * надо ли отправлять типа ok? если я пишу и клиента и сервер
 */
// struct Handshake{
//     Handshake(){
//         loadUuid();
//     }
//     ~Handshake(){
//         saveUuid();
//     }
//     std::array<uint8_t, 16> uuid_;

//     void loadUuid(){
//         uuid_ = generateUuid();
//         // save datetime?
//         // load and save client session uuid
//         // if (!read_session_uuid("client_session_uuid", uuid_)){
//         //     uuid_ = generateUuid();
//         //     saveUuid();
//         // }
//     }
//     void saveUuid(){
//         // write_session_uuid(uuid_, "client_session_uuid");
//     }
// };

// общий простой епол без сокетов - прокидывает события
class BaseEpoll {
public:
    BaseEpoll();
    virtual ~BaseEpoll();

    bool AddFd(int fd);
    void RemoveFd(int fd);
    void RunEpoll(bool connectInLoop = false);
    void StopEpoll();

    // void StartEpoll(bool autoreconnect);

    // прокидывает все вызовы сюда
    // void (*on_event_handlers)(int fd, uint32_t events) = 0;

    void SetOnReadAcceptHandler(std::function<void(int)> handler) { on_read_ = std::move(handler); };
    void SetOnReadyWriteHandler(std::function<void(int)> handler) { on_write_ = std::move(handler); };
    void SetOnDisconnectHandler(std::function<void(int)> handler) { on_hangup_ = std::move(handler); };
private:
    int epfd_ = -1;

    std::function<void(int)> on_read_;
    std::function<void(int)> on_write_;
    std::function<void(int)> on_hangup_;

    void ExecLoop();// блокирует
    std::unique_ptr<std::thread> thLoop_ = 0;
    bool need_stop_ = false;
};




#endif // EPOLL_H
