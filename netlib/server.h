#ifndef SERVER_H
#define SERVER_H

#include "const.h"
#include "stats.h"

struct ServerConfig{
    string host = "0.0.0.0";
    int port = 12345;
    string filename;// empty = without write

    int recv_buffer_size = 1 * 1024 * 1024; // 100 MiB
    int max_connections = 10;
};

enum class ServerState : uint8_t{
    STOPPED, // default
    STARTING,
    RUNNING,
    STOPPING,
    ERROR
};

class IServer{
public:
    virtual ~IServer() = default;
    virtual bool start() = 0; // wait accept

    ServerConfig conf_;
    string last_error_;
    int socket_ = 0;
    ServerState state_ = ServerState::STOPPED;

    bool create_socket();
    string getServerState(ServerState state);

    Stats stats_;
};

class SinglethreadServer : public IServer {
public:
    SinglethreadServer(ServerConfig &conf){
        conf_ = std::move(conf);
    }
    bool start();

};

class MultithreadServer : public IServer {
public:
    MultithreadServer(ServerConfig &conf){
        conf_ = std::move(conf);
    }
    bool start();
};



#endif // SERVER_H
