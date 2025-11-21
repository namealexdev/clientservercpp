#ifndef ISERVER_H
#define ISERVER_H

#include <functional>
#include <memory>
#include <stdint.h>
#include "stats.h"

struct ServerConfig{
    string host = "0.0.0.0";
    int port = 12345;
    int max_connections = 10;
    int worker_threads = 0;
    // int recv_buffer_size = 1 * 1024 * 1024; // 100 MiB
};

enum class ServerState : uint8_t{
    STOPPED, // default
    WAITING,
    ERROR,
};

class IServer{
public:
    IServer(ServerConfig&& c) : conf_(std::move(c)) {};
    virtual ~IServer() = default;
    virtual bool StartListen() = 0; // wait accept
    virtual void Stop() = 0;
    virtual int CountClients() = 0;
    virtual void AddHandlerEvent(EventType type, std::function<void(void*)> handler) = 0;

    // для воркеров, тк храним iserver
    virtual double GetBitrate() = 0;
    virtual std::vector<std::unique_ptr<IServer>>* GetWorkers() = 0;
    virtual std::vector<Stats*> GetClientsStats() = 0;
    virtual void AddClientFd(int fd, const Stats &st) = 0;

    // Stats& GetStats(){return stats_;}
    std::string_view GetLastError(){return last_error_;}
    string GetServerState();
    ServerState ServerState();

protected:
    enum ServerState state_ = ServerState::STOPPED;
    int create_listen_socket();

    ServerConfig conf_;
    string last_error_;
    // Stats stats_;
};


#endif // ISERVER_H
