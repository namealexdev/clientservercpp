#ifndef CLIENT_H
#define CLIENT_H

#include "const.h"
#include "stats.h"
#include "epoll.h"

struct ClientConfig{
    string host;
    string server_ip = "127.0.0.1";
    uint16_t server_port = 12345;

    // bool auto_reconnect = false;
    // int serialization_ths = 1;
    // int send_buffer_size = 1 * 1024 * 1024; // 1 MiB
    // int recv_buffer_size = 1 * 1024 * 1024; // 1 MiB
};

enum class ClientState : uint8_t{
    DISCONNECTED = 0, // default
    RECONNECTED,
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

    virtual void Connect() = 0;
    virtual void Disconnect() = 0;

    string GetClientState();
    inline void setAutoSend(bool b){
        auto_send_ = b;
    }

    // прокидываем методы в LightEpoll
    virtual void SendToSocket(char* d, int sz) = 0;
    virtual void StartAsyncQueue() = 0;
    virtual void QueueAdd(char* d, int sz) = 0;
    virtual void QueueSend() = 0;

    virtual void AddHandlerEvent(EventType type, std::function<void(void*)> handler) = 0;

    ClientConfig conf_;
    string last_error_;
    // TODO: может state_ переместить в event dispatcher?
    ClientState state_ = ClientState::DISCONNECTED;
    Stats stats_;

protected:
    int create_socket_connect();
    bool auto_send_ = true;
};

class SimpleClient : public IClient{
public:
    SimpleClient(ClientConfig config);
    ~SimpleClient();

    void Connect();
    void Disconnect();

    void SendToSocket(char* data, int size);
    void QueueAdd(char* data, int size);
    void QueueSend();
    void StartAsyncQueue();

    void AddHandlerEvent(EventType type, std::function<void(void*)> handler);

private:
    void handleData();

    int socket_ = -1;
    EventDispatcher* dispatcher_ = 0;
    BaseEpoll epoll_;

    char buffer[BUF_SIZE];

    // WARN: поток и мутекс нужны только для мультипотока
    // TODO: change to lockfree
    std::thread* queue_th_;
    std::mutex queue_mtx_;
    std::queue<std::pair<char*,int>> queue_;
};


// class SinglethreadClient : public IClient, public IClientEventHandler {
// public:
//     SinglethreadClient(ClientConfig&& conf);
//     void connect();
//     void disconnect();

//     void send(char* d, int sz);
//     void queue_add(char* d, int sz);
//     void queue_send();

// private:
//     ClientLightEpoll epoll_;

//     void onEvent(EventType e);
// };

// class MultithreadClient : public IClient, public IClientEventHandler {
// public:
//     MultithreadClient(ClientConfig&& conf);
//     void connect();
//     void disconnect();

//     void send(char *d, int sz);
//     void queue_add(char *d, int sz);
//     void queue_send();

// private:
//     ClientMultithEpoll epoll_;
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
