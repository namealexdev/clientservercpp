#ifndef ICLIENT_H
#define ICLIENT_H

#include <functional>
#include <stdint.h>
#include "stats.h"

struct ClientConfig{
    string host;// to bind
    string server_ip = "127.0.0.1";
    uint16_t server_port = 12345;

    bool auto_send = false;
    bool auto_reconnect = false;
    // bool auto_reconnect = false;
    // int serialization_ths = 1;
    // int send_buffer_size = 1 * 1024 * 1024; // 1 MiB
    // int recv_buffer_size = 1 * 1024 * 1024; // 1 MiB
};

enum class ClientState : uint8_t{
    DISCONNECTED = 0, // default
    CONNECTING,
    HANDSHAKE,
    WAITING,
    SENDING,
    ERROR,
};

// struct ConnectionMethods{
//     virtual void send(char* d, int sz) = 0;
//     virtual void queue_add(char* d, int sz) = 0;
//     virtual void queue_send() = 0;
//     virtual int recv(char** d) = 0;
//     virtual bool wait_accept() = 0;
// };

/*
клиент может быть однопоточным, то есть send делается сразу (без очереди)
  или queue_add и queue_send надо вызывать самому
  1th send | queue_add + queue_send
многопоточный записывает в очередь в главном, а отправку всегда делаем в своем потоке
  1th send | 1th queue_add - 2th queue_send
*/
class IClient {
public:
    IClient(ClientConfig&& c) : conf_(std::move(c)) {};
    virtual ~IClient() = default;

    virtual void Start() = 0;
    // virtual void StartWaitConnect() = 0;
    virtual void Stop() = 0;
    bool IsConnected()
    {
        return state_ == ClientState::WAITING || state_ == ClientState::SENDING;
    }

    string GetClientState();
    // Wait until the client connects (WAITING or SENDING) or ERROR/timeout occurs.
    // Returns true on WAITING/SENDING; false on timeout or ERROR.
    bool wait_connecting(int timeout_ms);
    // inline void SetAutoSend(bool b){
    //     auto_send_ = b;
    // }

    // прокидываем методы в LightEpoll
    virtual int SendToSocket(char* d, uint32_t sz) = 0;
    virtual void SwitchAsyncQueue(bool enable) = 0;
    virtual bool QueueAdd(char* d, int sz) = 0;
    virtual bool QueueSendAll() = 0;

    virtual void AddHandlerEvent(EventType type, std::function<void(void*)> handler) = 0;

    Stats& GetStats(){return stats_;}
    std::string_view GetLastError(){return last_error_;}

protected:
    // void set_state(ClientState s){
    //     {
    //         std::lock_guard<std::mutex> lk(state_mtx_);
    //         state_ = s;
    //     }
    //     state_cv_.notify_all();
    // }

    int create_socket_connect();
    bool auto_send_ = true;

    ClientConfig conf_;
    string last_error_;
    // TODO: может state_ переместить в event dispatcher?
    ClientState state_ = ClientState::DISCONNECTED;
    Stats stats_;// считаем отправку

    std::array<uint8_t, 16> uuid_;

    // std::mutex state_mtx_;
    // std::condition_variable state_cv_;
};

#endif // ICLIENT_H
