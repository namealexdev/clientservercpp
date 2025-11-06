#include "server.h"
#include "serialization.h"

int IServer::create_listen_socket()
{
    int sock;
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        last_error_ = "socket failed";
        return -1;
    }

    // if (setsockopt(sock, SOL_SOCKET, SO_RCVBUF, &conf_.recv_buffer_size, sizeof(conf_.recv_buffer_size)) == -1) {
    //     std::cout << "fail set SO_RCVBUF";
    // };

    timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) == -1) {
        std::cout << "fail set SO_RCVTIMEO 1s";
    }

    int opt = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        last_error_ = "setsockopt reuseaddr failed";
        close(sock);
        return -1;
    }

    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_port = htons(conf_.port);
    if (inet_pton(AF_INET, conf_.host.c_str(), &address.sin_addr) <= 0) {
        last_error_ = "inet_pton failed " + conf_.host;
        close(sock);
        return -1;
    }

    if (bind(sock, (struct sockaddr*)&address, sizeof(address)) < 0) {
        last_error_ = "bind failed";
        close(sock);
        return -1;
    }

    if (listen(sock, conf_.max_connections) < 0) {
        last_error_ = "listen failed";
        close(sock);
        return false;
    }

    return sock;
}

string IServer::getServerState()
{
    switch (state_) {
    case ServerState::STOPPED: return "STOPPED";
    case ServerState::WAITING: return "WAITING";
    case ServerState::ERROR: return "ERROR: " + last_error_;
    default: return "UNKNOWN";
    }
}

bool SinglethreadServer::start()
{
    auto sock = create_listen_socket();
    if (sock < 0){
        state_ = ServerState::ERROR;
        return false;
    }

    state_ = ServerState::WAITING;

    epoll_.start_handle(sock);
    return true;
}

void SinglethreadServer::stop()
{
    state_ = ServerState::STOPPED;
    epoll_.stop();
}

int SinglethreadServer::countClients()
{
    return epoll_.countClients();
}

void SinglethreadServer::onEvent(EventType e){
    switch(e){
    case EventType::ClientDisconnect:
        d("detect client disconnected");
        break;
    default:
        break;
    }
    d("srv onEvent " << (int)e << " state:" << (int)state_)
}


// bool MultithreadServer::start()
// {
//     //accept in n threads
// }
