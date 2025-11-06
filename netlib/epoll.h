#ifndef EPOLL_H
#define EPOLL_H

#include <functional>
#include <sys/epoll.h>
#include <atomic>
#include "const.h"
#include "stats.h"


enum class EventType {
    Disconnected,
    Reconnected,
    Waiting,

    ClientDisconnect
};


class IClientEventHandler {
public:
    virtual ~IClientEventHandler() = default;
    virtual void onEvent(EventType e) = 0;
};


// общий простой епол без сокетов

class IEpoll {
protected:
    IEpoll();
    ~IEpoll();

    IClientEventHandler* clientHandler_ = nullptr;

    // template<typename Derived>
    void exec();// блокирует
    // прокидывает все вызовы сюда
    // void (*on_event_handlers)(int fd, uint32_t events) = 0;
    std::function<void(int fd, uint32_t events)> on_event_handlers = 0;

    bool add_fd(int fd, uint32_t events);
    void remove_fd(int fd);

    bool need_stop_ = false;

private:
    int epfd_ = -1;
    static const int MAX_EVENTS = 64;
};

// for client
class ClientLightEpoll : protected IEpoll
{
public:
    // using HandlerPtr = void (LightEpoll::*)(int, uint32_t);
    // HandlerPtr handler_ptr = &LightEpoll::event_handlers;
    // (this->*handler_ptr)(fd, evs);

    ClientLightEpoll(IClientEventHandler* clh);
    void start_handle(int sock);
    void stop();

    bool need_reconnect_ = false;
    // std::function<void(const char* data, ssize_t size)> on_recv_handler = 0;

    //ВЫНЕСТИ ЭТО в класс для send recv
    // now in socket
    void send(char* d, int sz);
    // using queue or lockfree queue
    void queue_add(char* d, int sz);
    // get from q and call send
    void queue_send();


private:
    void on_epoll_event(int fd, uint32_t evs);
    void handle_socket_data();

    std::thread* handleth_ = 0;
    static constexpr size_t BUF_SIZE = 65536;
    char buffer[BUF_SIZE];
    int socket_ = -1;

    std::queue<std::pair<char*,int>> queue_;
};

// должен быть тем же что и ClientLightEpoll
// но для сервера, то есть делать accept
// то что добавили: countClients, clients handle_accept
class ServerLightEpoll : protected IEpoll
{
public:
    ServerLightEpoll(IClientEventHandler* clh);

    void start_handle(int sock);
    void stop();
    int countClients();

private:
    void on_epoll_event(int fd, uint32_t evs);

    void remove_client(int fd);
    void handle_client_data(int fd);
    void handle_accept();

    std::thread* handleth_ = 0;
    int socket_ = -1;
    static constexpr size_t BUF_SIZE = 65536;
    char buffer[BUF_SIZE];

    std::unordered_map<int, Stats> clients;
};


// wait th + queue send th
// добавляем асинхронную очередь пакетов
class ClientMultithEpoll : protected IEpoll
{
    ClientMultithEpoll(IClientEventHandler* clh);
    void start_handle(int sock);
    void stop();

    bool need_reconnect_ = false;

    //ВЫНЕСТИ ЭТО в класс для send recv
    // now in socket
    void send(char* d, int sz);
    // using queue or lockfree queue
    void queue_add(char* d, int sz);
    // get from q and call send
    void queue_send();

private:
    void on_epoll_event(int fd, uint32_t evs);
    void handle_socket_data();

    std::thread* handleth_ = 0;
    static constexpr size_t BUF_SIZE = 65536;
    char buffer[BUF_SIZE];
    int socket_ = -1;

    void start_queue();
    std::thread* queue_th_;
    std::mutex mtx_queue_;
    std::queue<std::pair<char*,int>> queue_;
};

// wait th + n th recv clns
// добавляем распределение по потокам
class ServerSubEpoll : protected IEpoll
{
public:
    std::atomic_int size_clients = 0;
    int countClients(){
        return size_clients;
    }

    // очередь для передачи сокетов между потоками
    void push_external_socket(int client_fd, const Stats &st){
        {
            std::lock_guard lock(mtx_pending_new_socks_);
            pending_new_socks_.push(std::make_pair(client_fd, std::move(st)));
            // добавляем тут, чтобы балансировка проходила корректно
            size_clients++;
        }
        uint64_t one = 1;
        write(wakeup_fd, &one, sizeof(one)); // разбудить epoll
    };
    int wakeup_fd = -1; // для пробуждения epoll
    std::queue<std::pair<int, Stats>> pending_new_socks_; // новые сокеты от MainEpoll
    std::mutex mtx_pending_new_socks_; // защищает очередь
};

// struct ThreadData {
//     ServerSubEpoll* subepoll;
//     int core_id;

//     static void* worker_thread_func(void* arg) {
//         ThreadData* data = static_cast<ThreadData*>(arg);

//         // Привязываем поток к конкретному ядру
//         // SetAffinityMask(data->core_id);

//         data->subepoll->exec();

//         delete data->subepoll;
//         delete data;

//         return nullptr;
//     }
// };

class ServerMultithEpoll : protected IEpoll
{
public:
    ServerMultithEpoll(){
        size_t num_workers = 2;

        pthread_t thread;
        pthread_attr_t attr;
        pthread_attr_init(&attr);

        for (size_t i = 0; i < num_workers; ++i) {
            auto* subepoll = new ServerSubEpoll();

            // Создаем поток с привязкой к ядру
            // ThreadData* thread_data = new ThreadData{subepoll, static_cast<int>(i + 1)};
            // if (pthread_create(&thread, &attr, &ThreadData::worker_thread_func, thread_data) == 0) {
            //     workers_.emplace_back(thread);
            //     subepolls_.emplace_back(subepoll);
            //     // thread_data_storage_.emplace_back(thread_data);
            // } else {
            //     delete subepoll;
            //     delete thread_data;
            //     std::cerr << "Failed to create thread " << i << std::endl;
            // }
        }

        pthread_attr_destroy(&attr);
    }
    ~ServerMultithEpoll() {
        for (auto& thread : workers_) {
            pthread_join(thread, nullptr);
        }
        subepolls_.clear();
        workers_.clear();
    }

    bool balance_socket(int client_fd, Stats &st){
        // нужен мутекс на балансере

        auto* min_subepoll = *std::min_element (subepolls_.begin(), subepolls_.end(), [](auto* a, auto* b) {
            return a->countClients() < b->countClients();
        });

        // if (count_clients() < min_subepoll->count_clients()){
        //     return false;// тут делаем
        // }
        min_subepoll->push_external_socket(client_fd, st);
        return true;
    }

    std::vector<pthread_t> workers_;
    std::vector<ServerSubEpoll*> subepolls_;

};



// Многопоточный epoll
// class MultiThreadEpoll {
// public:
//     MultiThreadEpoll();
//     ~MultiThreadEpoll();

//     bool init();
//     bool add_fd(int fd, uint32_t events);
//     bool remove_fd(int fd);
//     void run();

//     // Колбеки - вызываются в worker threads
//     std::function<void(int fd, uint32_t events)> on_event;

// private:
//     // Внутренняя многопоточная реализация
//     class ThreadPool {
//         // твоя многопоточная логика из MainEpoll
//     };

//     std::unique_ptr<ThreadPool> thread_pool_;
// };











// class ClientEpoll : public Epoll{
//     ClientEpoll(){
//     }
//     // клиенты
// };

// class ServerEpoll : public Epoll{
// };

#endif // EPOLL_H
