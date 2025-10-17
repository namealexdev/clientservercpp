#ifndef CLIENT_H
#define CLIENT_H

#include "const.h"

struct ClientConfig{
    string host;
    string server_ip;
    int server_port;
    // string session_uuid; // empty for new or try restore
};

enum class ClientState : uint8_t{
    UNINITIALIZED,
    INITIALIZED,
    CONNECTING,
    CONNECTED,
    AUTHENTICATING,
    READY,
    DISCONNECTING,
    DISCONNECTED,
    ERROR
};

class IClient {
public:
    virtual ~IClient() = default;
    virtual void start() = 0;

    ClientConfig conf_;
    string last_error_;
    int socket_ = 0;
    ClientState state_;

    bool create_socket(){
        int sock;
        if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
            last_error_ = "socket failed";
            return false;
        }

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
};


class SinglethreadClient : public IClient {
public:
    SinglethreadClient(ClientConfig &conf){
        conf_ = std::move(conf);
    }
    void start() {

        // write to server
        while (true) {
            if (!create_socket()){
                return ;
            }

            std::string message;

            int count = getRandomNumber(1, 10);
            for(int i = 0; i < count; i++) {

                message = "random message " + std::to_string(i) + " ";
                if (send(socket_, message.c_str(), message.length(), 0) <= 0) {
                    last_error_ = "failed to send";
                    break;
                }
            }

            close(socket_);
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
};

class MultithreadClient : public IClient {
public:
    MultithreadClient(ClientConfig &conf){
        conf_ = std::move(conf);
    }
    void start() {
        // do some in thread - async
    }
};



// string clientStateToString(ClientState state) {
//     switch (state) {
//     case ClientState::UNINITIALIZED: return "UNINITIALIZED";
//     case ClientState::INITIALIZED: return "INITIALIZED";
//     case ClientState::CONNECTING: return "CONNECTING";
//     case ClientState::CONNECTED: return "CONNECTED";
//     case ClientState::AUTHENTICATING: return "AUTHENTICATING";
//     case ClientState::READY: return "READY";
//     case ClientState::DISCONNECTING: return "DISCONNECTING";
//     case ClientState::DISCONNECTED: return "DISCONNECTED";
//     case ClientState::ERROR: return "ERROR";
//     default: return "UNKNOWN";
//     }
// }


#endif // CLIENT_H
