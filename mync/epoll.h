#ifndef EPOLL_H
#define EPOLL_H

#include <algorithm>
#include <atomic>
#include <mutex>
#include <queue>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <unordered_map>
#include <vector>
#include "stats.h"

#define d(x) std::cout << x << std::endl;
const size_t BUF_SIZE = 65536;//1024;
#include <pthread.h>

class Epoll {
    int epfd;
    // -1 - disable
    int sockfd = -1;
    bool is_listen;
    bool show_timer_stats;
    int timerfd = -1;

    std::unordered_map<int, Stats> clients;// todo переделать id?
    time_t start_time;

    char buffer[BUF_SIZE];// 65536 65Kb
    bool stdin_closed = false;
    bool socket_closed = false;

    inline int create_timer();
    inline void setup_epoll();


    bool add_fd(int fd, uint32_t events);

    inline void handle_stdin();
    inline void handle_client_data(int fd);
    inline void handle_socket_data();
    inline void accept_connections();
    inline void handle_timer();

    void remove_client(int fd);

public:
    Epoll(int sock, bool listen, int show_timer_stats);
    ~Epoll();

    void exec();

    virtual bool balance_socket(int client_fd, Stats &st){return false;};
    virtual void show_shared_stats(){};

    // тут только читаем это, не нужен атомик?
    std::atomic_int size_clients = 0;
    int count_clients(){
        return size_clients;
    }
    // return total bps
    uint64_t print_clients_stats(){
        uint64_t total_bps = 0;
        for (auto& c: clients){
            std::cout << "\n" << c.second.get_stats();
            total_bps += c.second.current_bps;
        }
        return total_bps;
    }
    int core_id = -1;

    // очередь для передачи сокетов между потоками
    void push_external_socket(int client_fd, const Stats &st);
    int event_external_fd = -1; // для пробуждения epoll
    std::queue<std::pair<int, Stats>> pending_new_socks_; // новые сокеты от MainEpoll
    std::mutex mtx_pending_new_socks_; // защищает очередь

};

// @return On success, these functions return 0
inline static int SetAffinityMask(int core_id) noexcept {
    if (core_id < 0)
        return -1;

    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);
    return pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
}

struct ThreadData {
    Epoll* subepoll;
    int core_id;



    static void* worker_thread_func(void* arg) {
        ThreadData* data = static_cast<ThreadData*>(arg);

        // Привязываем поток к конкретному ядру
        SetAffinityMask(data->core_id);

        data->subepoll->exec();

        delete data->subepoll;
        delete data;

        return nullptr;
    }
};


class MainEpoll : public Epoll{
public:
    MainEpoll(int sock, bool isl) : Epoll(sock, isl, true)
    {
        if (SetAffinityMask(0) < 0){
            std::cout << "fail set main process to 1 core" << std::endl;
        }
        size_t num_workers = 4;

        pthread_t thread;
        pthread_attr_t attr;
        pthread_attr_init(&attr);

        for (size_t i = 0; i < num_workers; ++i) {
            Epoll* subepoll = new Epoll(-1, false, false);
            subepolls_.emplace_back(subepoll);

            // Создаем поток с привязкой к ядру
            ThreadData* thread_data = new ThreadData{subepoll, static_cast<int>(i + 1)};
            if (pthread_create(&thread, &attr, &ThreadData::worker_thread_func, thread_data) == 0) {
                workers_.emplace_back(thread);
            } else {
                delete thread_data;
                std::cerr << "Failed to create thread " << i << std::endl;
            }
        }

        pthread_attr_destroy(&attr);
    }

    // true отдали, false забирай себе на обработку
    // это тормозит, но только на 1 listener
    bool balance_socket(int client_fd, Stats &st){
        // проверяем
        Epoll* min_subepoll = *std::min_element (subepolls_.begin(), subepolls_.end(), [](Epoll* a, Epoll* b) {
            return a->count_clients() < b->count_clients();
        });
        // if (count_clients() < min_subepoll->count_clients()){
        //     // надо добавить в этот и разбудить его
        //     push_external_socket(client_fd, st);
        //     return false;// тут делаем
        // }
        min_subepoll->push_external_socket(client_fd, st);
        return true;
    }

    void show_shared_stats(){

        int c = 1;
        int all_clients = count_clients();
        std::cout << "main" << "-" << all_clients << ", ";
        for (auto& e: subepolls_){
            int cl = e->count_clients();
            all_clients += cl;
            std::cout << c ++ << "-" << cl << ", ";
        }
        std::cout  << " = " << all_clients << std::endl;

        uint64_t total_bps = 0;
        for (Epoll* e: subepolls_){
            total_bps += e->print_clients_stats();
        }

        // total
        std::cout << "\n\t\ttotal:\t\t" << Stats::formatValue(total_bps, "bps") << std::endl;
        std::cout << std::endl;
    }

private:
    std::vector<pthread_t> workers_;
    std::vector<Epoll*> subepolls_;
};

void listen_mode_epoll(int port);
void client_mode_epoll(std::string host, int port);

#endif // EPOLL_H
