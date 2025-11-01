#include "client.h"
#include "serialization.h"

bool IClient::create_socket_connect()
{
    int sock;
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        last_error_ = "socket failed";
        return false;
    }

    // setsockopt(sock, SOL_SOCKET, SO_SNDBUF, &conf_.send_buffer_size, sizeof(conf_.send_buffer_size));

    if (!conf_.host.empty()){
        struct sockaddr_in bind_addr;
        bind_addr.sin_family = AF_INET;
        if (inet_pton(AF_INET, conf_.host.c_str(), &bind_addr.sin_addr) <= 0) {
            last_error_ = "inet_pton bind failed " + conf_.host;
            close(sock);
            return false;
        }
        if (bind(sock, (struct sockaddr*)&bind_addr, sizeof(bind_addr)) < 0) {
            last_error_ = "bind failed";
            close(sock);
            return false;
        }
    }

    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(conf_.server_port);
    if (inet_pton(AF_INET, conf_.server_ip.c_str(), &serv_addr.sin_addr) <= 0) {
        last_error_ = "inet_pton failed " + conf_.host;
        close(sock);
        return false;
    }

    if (::connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        last_error_ = "connection failed";
        close(sock);
        return false;
    }

    socket_ = sock;
    return true;
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

void SinglethreadClient::connect()
{
    if (!create_socket_connect()){
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
}

void SinglethreadClient::disconnect()
{
    close(socket_); socket_ = -1;
    state_ = ClientState::DISCONNECTED;
}

void MultithreadClient::connect()
{

}

void MultithreadClient::disconnect()
{

}
