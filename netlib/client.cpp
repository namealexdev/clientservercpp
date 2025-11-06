#include "client.h"
#include "serialization.h"

int IClient::create_socket_connect()
{
    int sock;
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        last_error_ = "socket failed";
        return -1;
    }

    // setsockopt(sock, SOL_SOCKET, SO_SNDBUF, &conf_.send_buffer_size, sizeof(conf_.send_buffer_size));

    if (!conf_.host.empty()){
        struct sockaddr_in bind_addr;
        bind_addr.sin_family = AF_INET;
        if (inet_pton(AF_INET, conf_.host.c_str(), &bind_addr.sin_addr) <= 0) {
            last_error_ = "inet_pton bind failed " + conf_.host;
            close(sock);
            return -1;
        }
        if (bind(sock, (struct sockaddr*)&bind_addr, sizeof(bind_addr)) < 0) {
            last_error_ = "bind failed";
            close(sock);
            return -1;
        }
    }

    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(conf_.server_port);
    if (inet_pton(AF_INET, conf_.server_ip.c_str(), &serv_addr.sin_addr) <= 0) {
        last_error_ = "inet_pton failed " + conf_.host;
        close(sock);
        return -1;
    }

    if (::connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        last_error_ = "connection failed";
        close(sock);
        return -1;
    }

    return sock;
}

string IClient::getClientState()
{
    switch (state_) {
    case ClientState::DISCONNECTED: return "DISCONNECTED";
    case ClientState::HANDSHAKE: return "HANDSHAKE";
    case ClientState::WAITING: return "WAITING";
    case ClientState::SENDING: return "SENDING";
    case ClientState::ERROR: return "ERROR: " + last_error_;
    default: return "UNKNOWN";
    }
}

SinglethreadClient::SinglethreadClient(ClientConfig &&conf) : IClient(std::move(conf)), epoll_(this){
    // conf_ = std::move(conf);
    // loadUuid();
    // epoll_.on_event = onEvent
}

void SinglethreadClient::connect()
{
    auto sock = create_socket_connect();
    if (sock < 0){
        state_ = ClientState::ERROR;
        return ;
    }


    state_ = ClientState::HANDSHAKE;

    // if (!auth()){
    //     close(sock);
    //     return false;
    // }

    // TODO: если в очереди есть данные отправляем
    // if (auto_send_ && !queue.empty()){
    //     // qsend()
    // }


    state_ = ClientState::WAITING;
    epoll_.start_handle(sock);
}

void SinglethreadClient::disconnect()
{
    state_ = ClientState::DISCONNECTED;
    epoll_.stop();
}

void SinglethreadClient::send(char *d, int sz){epoll_.send(d, sz);}

void SinglethreadClient::queue_add(char *d, int sz){epoll_.queue_add(d, sz);}

void SinglethreadClient::queue_send(){epoll_.queue_send();}

void SinglethreadClient::onEvent(EventType e){

    switch(e){
    case EventType::Disconnected:
        state_ = ClientState::DISCONNECTED;
        break;
    case EventType::Reconnected:
        state_ = ClientState::RECONNECTED;
        break;
    case EventType::Waiting:
        state_ = ClientState::WAITING;
        break;
    default:
        break;
    }
    d("cl onEvent " << (int)e << " state:" << (int)state_)
}

// void MultithreadClient::connect()
// {

// }

// void MultithreadClient::disconnect()
// {

// }
