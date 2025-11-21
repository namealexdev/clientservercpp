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

class IClient {
public:
    IClient(ClientConfig&& c) : conf_(std::move(c)) {};
    virtual ~IClient() = default;

    virtual void Start() = 0;
    // virtual void StartWaitConnect() = 0;
    virtual void Stop() = 0;
    bool IsConnected(){
        return state_ == ClientState::WAITING || state_ == ClientState::SENDING;
    }

    virtual int SendToSocket(char* d, uint32_t sz) = 0;
    virtual void SwitchAsyncQueue(bool enable) = 0;
    virtual bool QueueAdd(char* d, int sz) = 0;
    virtual bool QueueSendAll() = 0;

    virtual void AddHandlerEvent(EventType type, std::function<void(void*)> handler) = 0;

    ClientState ClientState();
    string GetClientState();
    Stats& GetStats(){return stats_;}
    std::string_view GetLastError(){return last_error_;}

protected:
    int create_socket_connect();

    ClientConfig conf_;
    string last_error_;
    enum ClientState state_ = ClientState::DISCONNECTED;

    Stats stats_;
};

#endif // ICLIENT_H
