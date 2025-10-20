#ifndef CLIENT_H
#define CLIENT_H

#include "const.h"
#include "stats.h"

struct ClientConfig{
    string host;
    string server_ip = "127.0.0.1";
    int server_port = 12345;

    int send_buffer_size = 1 * 1024 * 1024; // 100 MiB
};

enum class ClientState : uint8_t{
    DISCONNECTED = 0, // default
    CONNECTING,
    ERROR,
    AUTHENTICATING,
    READY,
};

class IClient {
public:
    virtual ~IClient() = default;
    virtual void start() = 0;

    ClientConfig conf_;
    string last_error_;
    int socket_ = 0;
    ClientState state_ = ClientState::DISCONNECTED;

    bool create_socket_connect();
    string getClientState(ClientState state);

    std::array<uint8_t, 16> uuid_;
    void loadUuid(){
        // save datetime?
        // load and save client session uuid
        read_session_uuid("client_session_uuid", uuid_);
        if (uuid_.empty()){
            uuid_ = generateUuid();
            write_session_uuid(uuid_, "client_session_uuid");
        }
    }

// private:
    bool auth();

    Stats stats_;
};


class SinglethreadClient : public IClient {
public:
    SinglethreadClient(ClientConfig &conf){
        conf_ = std::move(conf);
        loadUuid();
    }
    void start();
};

class MultithreadClient : public IClient {
public:
    MultithreadClient(ClientConfig &conf){
        conf_ = std::move(conf);
        loadUuid();
    }
    void start();
};


#endif // CLIENT_H
