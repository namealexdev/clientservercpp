#ifndef SERVER_H
#define SERVER_H

#include "const.h"
#include "epoll.h"

#include <cassert>
#include <condition_variable>
#include <mutex>

#include "libinclude/iserver.h"
#include "packetparser.h"

#pragma push_macro("d")
#undef d
#include <boost/lockfree/spsc_queue.hpp>
#pragma pop_macro("d")

struct ClientData{
    // только для handshake?
    // enum ClientState {
    //     HANDSHAKE,
    //     DATA
    // } state;
    // std::array<uint8_t, 16> client_uuid;

    Stats stats;
    PacketParser pktReader_;
    // очередь пакетов для клиента?
};


class SimpleServer : public IServer{
public:
    SimpleServer(ServerConfig config, EventDispatcher* e = nullptr);
    bool StartListen(int num_workers = 0);
    void StartWait();
    void Stop();
    int CountClients();

    // нужен для передачи сокетов в многопоточном сервере
    void AddClientFd(int fd, const Stats& st);

    void AddHandlerEvent(EventType type, std::function<void(void*)> handler);

    double GetBitrate(){
        // поидее только один
        std::vector<Stats*> stats = GetClientsStats();
        double btr = 0;
        int i = 1;
        for(auto& c: stats){
            // для accept сокета, тоже добавляем в клиенты чтобы статистику
            // if (c->ip.empty())continue;
            d(i++ << " " << c->ip << " " << c->getCalcBitrate() << " " << c->total_bytes);
            btr += c->getBitrate();
        }
        return btr;
        // Stats& stats = GetStats();
        // stats.calcBitrate();
        // d("sim get btr " << stats.getBitrate() << " " << stats.total_bytes << "(" << stats.ip << ")")
        // return stats.getBitrate();
    };
    std::vector<std::unique_ptr<IServer>>* GetWorkers(){
        return nullptr;
    };
    std::vector<Stats*> GetClientsStats(){
        std::vector<Stats*> vec;
        for (auto& c: clients_){
            vec.emplace_back(&c.second->stats);
        }
        return vec;
    }

private:
    void handleAccept(); // принимает клиентов
    void handleClientData(int fd); // обрабатывает данные от клиента
    void removeClient(int fd); // удаляет клиента из epoll и списка

    int listen_socket_ = -1;
    EventDispatcher* dispatcher_ = 0;
    BaseEpoll epoll_;
    char buffer_[BUF_READ_SIZE];

    std::unordered_map<int, std::shared_ptr<ClientData>> clients_;
    //for add client (from other thread when MultithreadServer) and erace
    std::mutex clients_mtx_;

    // boost::lockfree::queue<std::pair<int, Stats>> pending_clients_{1024};
    // int eventfd_add_client_ = -1;
};

class MultithreadServer : public IServer{
public:
    explicit MultithreadServer(ServerConfig config);

    bool StartListen(int num_workers = 1);
    void Stop();
    int CountClients();

    void AddClientFd(int fd, const Stats &st){
        // сюда не должно быть таких конектов
        assert(false);
        // accept_epoll_.AddFd(fd);
        // worker_client_counts_[0] ++;
        // preClient_socks_.push(std::make_pair(fd, std::move(st)));
    }

    double GetBitrate(){
        // d("multi get btr:")
        double bps = 0;
        int num_worker = 1;
        for (auto &w: workers_){
            std::vector<Stats*> stats = w->GetClientsStats();
            for(auto& c: stats){
                d(num_worker << "-  " << c->ip << " " << c->getCalcBitrate());
                bps += c->getBitrate();
            }
            num_worker++;
        }
        d("full " << Stats::formatBitrate(bps))
        return bps;
    };
    std::vector<std::unique_ptr<IServer>>* GetWorkers(){
        return &workers_;
    };
    std::vector<Stats*> GetClientsStats(){
        return {};
    }

    void AddHandlerEvent(EventType type, std::function<void(void*)> handler);

private:
    void handleAccept(int fd); // распределяет сокеты по воркерам

    int listen_socket_ = -1;
    EventDispatcher* dispatcher_ = 0;
    BaseEpoll accept_epoll_; // только для accept

    std::vector<std::unique_ptr<IServer>> workers_;
    std::vector<std::atomic_int> worker_client_counts_; // для балансировки(чтобы без atomic)
};

#endif // SERVER_H
