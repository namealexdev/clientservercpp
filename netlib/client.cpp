#include "client.h"

bool IClient::create_socket_connect()
{
    state_ = ClientState::CONNECTING;
    int sock;
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        last_error_ = "socket failed";
        state_ = ClientState::ERROR;
        return false;
    }

    if (!conf_.host.empty()){
        struct sockaddr_in bind_addr;
        bind_addr.sin_family = AF_INET;
        if (inet_pton(AF_INET, conf_.host.c_str(), &bind_addr.sin_addr) <= 0) {
            last_error_ = "inet_pton bind failed " + conf_.host;
            state_ = ClientState::ERROR;
            close(sock);
            return false;
        }
        if (bind(sock, (struct sockaddr*)&bind_addr, sizeof(bind_addr)) < 0) {
            last_error_ = "bind failed";
            state_ = ClientState::ERROR;
            close(sock);
            return false;
        }
    }

    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(conf_.server_port);
    if (inet_pton(AF_INET, conf_.server_ip.c_str(), &serv_addr.sin_addr) <= 0) {
        last_error_ = "inet_pton failed " + conf_.host;
        state_ = ClientState::ERROR;
        close(sock);
        return false;
    }

    if (::connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        last_error_ = "connection failed";
        state_ = ClientState::ERROR;
        close(sock);
        return false;
    }

    socket_ = sock;
    state_ = ClientState::AUTHENTICATING;

    // if (!auth()){
    //     state_ = ClientState::ERROR;
    //     close(sock);
    //     return false;
    // }

    state_ = ClientState::READY;
    return true;
}

// bool IClient::auth()
// {

//     return false;
// }

string IClient::getClientState(ClientState state)
{
    switch (state) {
    case ClientState::DISCONNECTED: return "DISCONNECTED";
    case ClientState::CONNECTING: return "CONNECTING";
    case ClientState::AUTHENTICATING: return "AUTHENTICATING";
    case ClientState::READY: return "READY";
    case ClientState::ERROR: return "ERROR: " + last_error_;
    default: return "UNKNOWN";
    }
}

void SinglethreadClient::start()
{
    if (!create_socket_connect()){
        return ;
    }

    new std::thread([&](){
        while(socket_){
            std::cout << getClientState(state_) << " send:" << stats_.getBitrate() << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(2));
        }
    });

    while (true) {
        std::string message;

        int count_send = 0;
        for(int i = 0; i < getRandomNumber(1, 10); i++) {

            message = "random message " + std::to_string(i) + " ";
            count_send = send(socket_, message.c_str(), message.length(), 0);
            if (count_send <= 0) {
                last_error_ = "failed to send";
                break;
            }
            stats_.addBytes(count_send);
        }

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    close(socket_);
    state_ = ClientState::DISCONNECTED;
}

void MultithreadClient::start()
{
    // асинхронное чтение и запись в очередь пакетов
    // тут чтение и отправка

}
