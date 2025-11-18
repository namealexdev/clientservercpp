#ifndef ISERVER_H
#define ISERVER_H

#include <functional>
#include <memory>
#include <stdint.h>
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
    virtual ~IServer() = default;
    virtual bool StartListen(int num_workers = 0) = 0; // wait accept
    virtual void Stop() = 0;
    virtual int CountClients() = 0;
    virtual void AddHandlerEvent(EventType type, std::function<void(void*)> handler) = 0;

    virtual double GetBitrate() = 0;
    virtual std::vector<std::unique_ptr<IServer>>* GetWorkers() = 0;
    virtual std::vector<Stats*> GetClientsStats() = 0;
    virtual void AddClientFd(int fd, const Stats &st) = 0;


    // Stats& GetStats(){return stats_;}
    std::string_view GetLastError(){return last_error_;}
    string GetServerState();

    // Wait until server enters WAITING state, or ERROR/timeout occurs.
    // Returns true if WAITING reached; false otherwise.
    // bool wait_started(int timeout_ms);

protected:
    // state and synchronization
    // void set_state(ServerState s){
    //     {
    //         std::lock_guard<std::mutex> lk(state_mtx_);
    //         state_ = s;
    //     }
    //     state_cv_.notify_all();
    // }
    ServerState state_ = ServerState::STOPPED;
    int create_listen_socket();

    ServerConfig conf_;
    string last_error_;
    // Stats stats_;

    // std::mutex state_mtx_;
    // std::condition_variable state_cv_;
};


#endif // ISERVER_H
