#ifndef CLIENT_H
#define CLIENT_H

#include "const.h"
#include "stats.h"

struct ClientConfig{
    string host;
    string server_ip = "127.0.0.1";
    int server_port = 12345;
    // bool auto_reconnect = false;
    // int serialization_ths = 1;
    // int send_buffer_size = 1 * 1024 * 1024; // 1 MiB
    // int recv_buffer_size = 1 * 1024 * 1024; // 1 MiB
};

enum class ClientState : uint8_t{
    DISCONNECTED = 0, // default
    HANDSHAKE,
    WAITING,
    SENDING,
    ERROR,
};

// struct Handshake{
//     std::array<uint8_t, 16> uuid_;

//     void loadUuid(){
//         // save datetime?
//         // load and save client session uuid
//         if (!read_session_uuid("client_session_uuid", uuid_)){
//             uuid_ = generateUuid();
//             saveUuid();
//         }
//     }
//     void saveUuid(){
//         write_session_uuid(uuid_, "client_session_uuid");
//     }
// };

#include <sys/epoll.h>

// общий простой епол
class IEpoll {
public:
    IEpoll()
    {
        start_time = time(nullptr);
        setup_epoll();
    }
    ~IEpoll()
    {
        fullstop();
        // if (epfd >= 0) close(epfd);
        // if (timerfd >= 0) close(timerfd);
        // if (wakeup_fd >= 0) close(wakeup_fd);
    }

    void fullstop();
    void exec()
    {
        const int MAX_EVENTS = 4;
        epoll_event events[MAX_EVENTS];
        const int EPOLL_TIMEOUT = 100;

        while (!need_stop && !socket_closed) {
            int nfds = epoll_wait(epfd, events, MAX_EVENTS, EPOLL_TIMEOUT);
            if (nfds == -1) {
                if (errno == EINTR) continue;
                throw std::runtime_error("epoll_wait");
            }

            for (int i = 0; i < nfds; ++i) {
                int fd = events[i].data.fd;
                uint32_t evs = events[i].events;

                // close socket
                if (evs & (EPOLLHUP | EPOLLRDHUP | EPOLLERR)) {
                    if (fd == sockfd) {
                        socket_closed = true;
                    } else {
                        // remove_client(fd);
                    }
                    event_close(fd);
                    continue;
                }

                if (evs & EPOLLIN) {
                    event_handlers(fd);
                }
            }
        }
    }

    int epfd;
    int sockfd = -1;
    bool socket_closed = false;
    bool need_stop = false;
    // добавление сокета
    inline void setup_epoll();
    inline bool add_fd(int fd, uint32_t events);

    // метрика для статистики
    time_t start_time;

    virtual void event_handlers(int fd) = 0;
    virtual void event_close(int fd) = 0;
};

// class ClientEpoll : public Epoll{
//     ClientEpoll(){
//     }
//     // клиенты
// };

// class ServerEpoll : public Epoll{
// };

bool IEpoll::add_fd(int fd, uint32_t events)
{
    epoll_event ev{.events = events, .data{.fd = fd}};
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev) == -1) {
        // throw std::runtime_error("epoll_ctl add");
        std::cout << "fail epoll_ctl add " << fd << std::endl;
        return false;
    }
    return true;
}

void IEpoll::setup_epoll(int s)
{
    sockfd = s;
    epfd = epoll_create1(EPOLL_CLOEXEC);
    if (epfd == -1) throw std::runtime_error("epoll_create1");

    // add_fd(STDIN_FILENO, EPOLLIN | EPOLLRDHUP);

    if (sockfd > 0){
        add_fd(sockfd, EPOLLIN | EPOLLRDHUP );//EPOLLET
    }

    // wakeup_fd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
    // if (wakeup_fd == -1) {
    //     throw std::runtime_error("eventfd");
    // }
    // add_fd(wakeup_fd, EPOLLIN);

    // // only for server and subserver
    // if (is_listen || sockfd == -1) {
    //     timerfd = create_timer();
    //     if (timerfd > 0)
    //         add_fd(timerfd, EPOLLIN);
    // }
}


//
class IClient {
public:
    IClient(ClientConfig&& c) : conf_(std::move(c)) {};
    virtual ~IClient() = default;
    virtual void connect() = 0;
    virtual void disconnect() = 0;

    string getClientState();
    bool auto_send_ = true;
    inline void setAutoSend(bool b){
        auto_send_ = b;
    }

    // in socket
    void send(char* d, int sz);
    // using queue or lockfree queue
    void send2q(char* d, int sz);
    // get from q and call send
    void qsend(char* d, int sz);

    ClientConfig conf_;
    string last_error_;
    int socket_ = 0;
    ClientState state_ = ClientState::DISCONNECTED;
    Stats stats_;

    bool create_socket_connect();
};


class SinglethreadClient : public IClient, public IEpoll {
public:
    SinglethreadClient(ClientConfig&& conf) : IClient(std::move(conf)){
        // conf_ = std::move(conf);
        // loadUuid();
    }
    void connect();
    void disconnect();
};

class MultithreadClient : public IClient {
public:
    MultithreadClient(ClientConfig&& conf) : IClient(std::move(conf)){
        // conf_ = std::move(conf);
        // loadUuid();
    }
    void connect();
    void disconnect();
};


#endif // CLIENT_H
