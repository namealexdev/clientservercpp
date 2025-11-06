#ifndef SERVER_H
#define SERVER_H

#include "const.h"
#include "epoll.h"
#include "stats.h"

struct ServerConfig{
    string host = "0.0.0.0";
    int port = 12345;
    // string filename;// empty = without write

    // int recv_buffer_size = 1 * 1024 * 1024; // 100 MiB
    int max_connections = 10;

    // int serialization_ths = 1;
};

enum class ServerState : uint8_t{
    STOPPED, // default
    WAITING,
    ERROR,
};

class IServer{
public:
    IServer(ServerConfig&& c) : conf_(std::move(c)) {};
    virtual bool start() = 0; // wait accept
    virtual void stop() = 0;
    virtual int countClients() = 0;

    ServerConfig conf_;
    string last_error_;
    Stats stats_;

    string getServerState();

protected:
    ServerState state_ = ServerState::STOPPED;
    int create_listen_socket();
};

class SinglethreadServer : public IServer, public IClientEventHandler {
public:
    SinglethreadServer(ServerConfig&& conf) :
        IServer(std::move(conf)), epoll_(this){

    }
    bool start();
    void stop();
    int countClients();

private:
    void onEvent(EventType e);

    ServerLightEpoll epoll_;
};

// class MultithreadServer : public IServer {
// public:
//     MultithreadServer(ServerConfig&& conf) : IServer(std::move(conf)){
//         // conf_ = std::move(conf);
//     }
//     bool start();
// };



#endif // SERVER_H
