#ifndef CLIENT_H
#define CLIENT_H

#include "const.h"
#include "stats.h"
#include "epoll.h"

struct ClientConfig{
    string host;
    string server_ip = "127.0.0.1";
    int server_port = 12345;
    // bool auto_reconnect = false;
    // int serialization_ths = 1;
    // int send_buffer_size = 1 * 1024 * 1024; // 1 MiB
    // int recv_buffer_size = 1 * 1024 * 1024; // 1 MiB
};

enum class ClientState : uint8_t{
    DISCONNECTED = 0, // default
    HANDSHAKE,
    WAITING,
    SENDING,
    ERROR,
};


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
    virtual void connect() = 0;
    virtual void disconnect() = 0;

    string getClientState();
    inline void setAutoSend(bool b){
        auto_send_ = b;
    }

    // прокидываем методы в LightEpoll
    void send(char* d, int sz);
    void queue_add(char* d, int sz);
    void queue_send(char* d, int sz);

    ClientConfig conf_;
    string last_error_;
    ClientState state_ = ClientState::DISCONNECTED;
    Stats stats_;

protected:
    // все что касается подключений, epoll
    // int socket_ = 0;

    int create_socket_connect();
    bool auto_send_ = true;
};


class SinglethreadClient : public IClient {
public:
    SinglethreadClient(ClientConfig&& conf) : IClient(std::move(conf)){
        // conf_ = std::move(conf);
        // loadUuid();
        // epoll_.on_recv_handler
    }
    void connect();
    void disconnect();

// private:
    ClientLightEpoll epoll_;
};

// class MultithreadClient : public IClient {
// public:
//     MultithreadClient(ClientConfig&& conf) : IClient(std::move(conf)){
//         // conf_ = std::move(conf);
//         // loadUuid();
//     }
//     void connect();
//     void disconnect();

// private:
//     MultiThreadEpoll epoll_;
// };

// struct Handshake{
//     std::array<uint8_t, 16> uuid_;

//     void loadUuid(){
//         // save datetime?
//         // load and save client session uuid
//         if (!read_session_uuid("client_session_uuid", uuid_)){
//             uuid_ = generateUuid();
//             saveUuid();
//         }
//     }
//     void saveUuid(){
//         write_session_uuid(uuid_, "client_session_uuid");
//     }
// };


#endif // CLIENT_H
