#ifndef SERVER_H
#define SERVER_H

#include "const.h"
#include "epoll.h"

#include <cassert>
#include <mutex>

#include "libinclude/iserver.h"
#include "packetparser.h"

#pragma push_macro("d")
#undef d
#include <boost/lockfree/spsc_queue.hpp>
#pragma pop_macro("d")

struct ClientData{
    Stats stats;
    PacketParser pktReader_;
    // очередь пакетов для клиента?
};


class SimpleServer : public IServer{
public:
    SimpleServer(ServerConfig config, EventDispatcher* e = nullptr);
    bool StartListen();
    void StartWait();
    void Stop();
    int CountClients();

    // нужен для передачи сокетов из MultithreadServer
    void AddClientFd(int fd, const Stats& st);

    void AddHandlerEvent(EventType type, std::function<void(void*)> handler);

    double GetBitrate();
    std::vector<std::unique_ptr<IServer>>* GetWorkers();
    std::vector<Stats*> GetClientsStats();

private:
    void handleAccept(); // принимает клиентов
    void handleClientData(int fd); // обрабатывает данные от клиента
    void removeClient(int fd); // удаляет клиента из epoll и списка

    // for send
    bool hasClientSocket(int fd);
    bool sendToSocket(int fd, char* data, uint32_t size);

    int listen_socket_ = -1;
    EventDispatcher* dispatcher_ = 0;
    BaseEpoll epoll_;
    char buffer_[BUF_READ_SIZE];

    std::unordered_map<int, std::shared_ptr<ClientData>> clients_;
    //for add client (from other thread when MultithreadServer) and erace
    std::mutex clients_mtx_;
};

/*
 * как будет bidirectional communication
 * нам приходит событие на то что получили данные и мы записываем ответ сразу
 * то есть можно передавать ссылку на сокет
 *
 * на всякий случай сделаем поиск
 */

class MultithreadServer : public IServer{
public:
    explicit MultithreadServer(ServerConfig config);

    bool StartListen();
    void Stop();
    int CountClients();

    void AddClientFd(int fd, const Stats &st);

    // send: search worker by client_fd
    // int _SendToClient(int client_fd, char* data, uint32_t size);

    double GetBitrate();
    std::vector<std::unique_ptr<IServer>>* GetWorkers();
    std::vector<Stats*> GetClientsStats();

    void AddHandlerEvent(EventType type, std::function<void(void*)> handler);

private:
    void handleAccept(int fd); // распределяет сокеты по воркерам

    int listen_socket_ = -1;
    EventDispatcher* dispatcher_ = 0;
    BaseEpoll accept_epoll_;

    std::vector<std::unique_ptr<IServer>> workers_;
    // для балансировки клиентов из воркеров
    std::vector<std::atomic_int> worker_client_counts_;
};

#endif // SERVER_H
