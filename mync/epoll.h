#ifndef EPOLL_H
#define EPOLL_H

#include "utils.h"
#include <sys/epoll.h>
#include <unordered_map>
#include "stats.h"

const int timer_timeout_secs = 1;


#include <sys/timerfd.h>
int create_timer() {
    int timer_fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    if (timer_fd == -1) {
        perror("timerfd_create");
        return -1;
    }

    // Настройка таймера на 1 секунду
    itimerspec timer_spec{};
    timer_spec.it_value.tv_sec = 5;    // Первый сработает через 5 сек
    timer_spec.it_value.tv_nsec = 0;
    timer_spec.it_interval.tv_sec = 5; // Повторять каждую секунду
    timer_spec.it_interval.tv_nsec = 0;

    if (timerfd_settime(timer_fd, 0, &timer_spec, nullptr) == -1) {
        perror("timerfd_settime");
        close(timer_fd);
        return -1;
    }

    return timer_fd;
}

class Epoll {
    int epfd;
    int sockfd;
    bool is_listen;
    int timerfd = -1;
    std::unordered_map<int, Stats> clients;
    time_t start_time;
    char buffer[65536];
    bool stdin_closed = false;
    bool socket_closed = false;

    void setup_epoll() {
        epfd = epoll_create1(EPOLL_CLOEXEC);
        if (epfd == -1) throw std::runtime_error("epoll_create1");

        add_fd(STDIN_FILENO, EPOLLIN | EPOLLRDHUP);
        add_fd(sockfd, EPOLLIN | EPOLLRDHUP);

        if (is_listen) {
            timerfd = create_timer();
            if (timerfd > 0)
                add_fd(timerfd, EPOLLIN);
        }
    }

    void add_fd(int fd, uint32_t events) {
        epoll_event ev{};
        ev.events = events;
        ev.data.fd = fd;
        if (epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev) == -1) {
            throw std::runtime_error("epoll_ctl add");
        }
    }

    void handle_stdin() {
        ssize_t n = read_from_stdin(buffer, sizeof(buffer));
        if (n > 0) {
            if (is_listen) {
                for (auto& client : clients) {
                    if (send(client.first, buffer, n, MSG_NOSIGNAL) != n) {
                        std::cerr << client.first << " send() failed: " << strerror(errno) << std::endl;
                    }
                }
            } else {
                if (send(sockfd, buffer, n, MSG_NOSIGNAL) != n) {
                    throw std::runtime_error("send to socket");
                }
            }
        } else if (n == 0) {
            shutdown(sockfd, SHUT_WR);
            stdin_closed = true;
        } else {
            throw std::runtime_error("read stdin");
        }
    }

    void handle_client_data(int fd) {
        ssize_t n = recv(fd, buffer, sizeof(buffer), 0);
        if (n > 0) {
            clients[fd].addBytes(n);
            if (write_to_stdout(buffer, n) != 0) {
                throw std::runtime_error("write to stdout");
            }
        } else {
            remove_client(fd);
        }
    }

    void handle_socket_data() {
        if (is_listen) {
            accept_connections();
        } else {
            // handle_client_data
            ssize_t n = recv(sockfd, buffer, sizeof(buffer), 0);
            if (n > 0) {
                if (write_to_stdout(buffer, n) != 0) {
                    throw std::runtime_error("write to stdout");
                }
            } else if (n == 0) {
                socket_closed = true;
            } else {
                throw std::runtime_error("recv from socket");
            }
        }
    }

    void accept_connections() {
        while (true) {
            sockaddr_in client_addr{};
            socklen_t addr_len = sizeof(client_addr);
            int client_fd = accept4(sockfd, (sockaddr*)&client_addr, &addr_len, SOCK_NONBLOCK | SOCK_CLOEXEC);

            if (client_fd == -1) {
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                    break;
                throw std::runtime_error("accept4");
            }

            add_fd(client_fd, EPOLLIN | EPOLLRDHUP);

            char ip_str[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &client_addr.sin_addr, ip_str, INET_ADDRSTRLEN);
            Stats st;
            st.ip = string(ip_str) + ":" + std::to_string(ntohs(client_addr.sin_port));
            clients.emplace(client_fd, std::move(st));
            std::cout << "Added socket: " << client_fd << " (" << clients[client_fd].ip << ")" << std::endl;
        }
    }

    void handle_timer() {
        uint64_t expirations;
        if (read(timerfd, &expirations, sizeof(expirations)) != sizeof(expirations)) return;

        time_t now = time(nullptr);
        std::cout << "======== Uptime: " << (now - start_time) << " s clients: " << clients.size() << "\n";

        uint64_t total_bps = 0;
        for (auto& [fd, stats] : clients) {
            stats.updateBps();
            std::string stats_msg;
            if (stats.checkFourGigabytes(stats_msg)) {
                send(fd, stats_msg.c_str(), stats_msg.size(), MSG_NOSIGNAL);
            }
            std::cout << stats.get_stats() << std::endl;
            total_bps += stats.current_bps;
        }

        if (total_bps > 0) {
            std::cout << "\t\ttotal:\t" << Stats::formatValue(total_bps, "bps") << std::endl;
        }
    }

    void remove_client(int fd) {
        epoll_ctl(epfd, EPOLL_CTL_DEL, fd, nullptr);
        clients.erase(fd);
        close(fd);
    }

public:
    Epoll(int sock, bool listen) : sockfd(sock), is_listen(listen) {
        start_time = time(nullptr);
        setup_epoll();
    }

    ~Epoll() {
        if (epfd >= 0) close(epfd);
        if (timerfd >= 0) close(timerfd);
    }

    void exec() {
        const int MAX_EVENTS = 4;
        epoll_event events[MAX_EVENTS];

        while (!stdin_closed && !socket_closed) {
            int nfds = epoll_wait(epfd, events, MAX_EVENTS, -1);
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
                        remove_client(fd);
                    }
                    continue;
                }

                if (evs & EPOLLIN) {
                    if (fd == STDIN_FILENO) handle_stdin();
                    else if (fd == timerfd) handle_timer();
                    else if (fd == sockfd) handle_socket_data();
                    else if (clients.count(fd)) handle_client_data(fd);
                }
            }
        }
    }
};

// #define d(x) std::cout << x << std::endl;
#define d(x) ;

void bidirectional_relay(int sockfd, bool islisten) {
    const int MAX_EVENTS = 4;// 2 fd консоль и сокет
    int epfd = epoll_create1(EPOLL_CLOEXEC);
    if (epfd == -1) {
        perror("epoll_create1");
        return;
    }

    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLRDHUP;// готов к чтению и закрыл соединение
    ev.data.fd = STDIN_FILENO;// консоль
    epoll_ctl(epfd, EPOLL_CTL_ADD, STDIN_FILENO, &ev);

    ev.data.fd = sockfd;// сокет
    epoll_ctl(epfd, EPOLL_CTL_ADD, sockfd, &ev);

    int timerfd = -1;
    if (islisten){
        timerfd = create_timer();
        ev.events = EPOLLIN;
        ev.data.fd = timerfd;
        int et = epoll_ctl(epfd, EPOLL_CTL_ADD, timerfd, &ev);
        std::cout << "timer " << timerfd << " " << et << std::endl;
    }

    time_t start = time(nullptr);

    char buf[65536];
    bool stdin_closed = false;
    bool socket_closed = false;
    std::unordered_map<int, Stats> clients;

    while (!stdin_closed && !socket_closed) {
        // std::cout << "while " << stdin_closed << " scl:" << socket_closed << std::endl;
        struct epoll_event events[MAX_EVENTS];
        int nfds = epoll_wait(epfd, events, MAX_EVENTS, -1);// блок
        if (nfds == -1) {
            if (errno == EINTR) continue;
            perror("epoll_wait");
            break;
        }

        for (int i = 0; i < nfds; ++i) {
            int fd = events[i].data.fd;
            uint32_t evs = events[i].events;

            // d(i <<" epoll " << fd << " " << evs)

            // Проверка на закрытие/ошибку
            //HUP-закрыт ERR-ошибка RDHUP-удаленно закрыл запись (узнаем без чтения)
            if (evs & (EPOLLHUP | EPOLLRDHUP | EPOLLERR)) {

                epoll_ctl(epfd, EPOLL_CTL_DEL, fd, nullptr);

                if (fd == sockfd){
                    d("stop main socket " << fd);
                    socket_closed = true;
                }else{
                    auto client_sock = clients.find(fd);
                    if (client_sock != clients.end()){
                        d("stop client " << fd)
                        clients.erase(fd);
                    }
                }
                continue;
            }

            if (timerfd > 0 && fd == timerfd){
                // Таймер сработал
                uint64_t expirations;
                ssize_t s = read(timerfd, &expirations, sizeof(expirations));
                if (s == sizeof(expirations)) {
                    // Выводим статистику
                    time_t end = time(nullptr);
                    // std::cout << "\n=== Stats at " << ctime(&end);
                    std::cout <<"======== Uptime: " << (end - start)
                              << " s clients: " << clients.size() << "\n";
                }

                // send stats if has 4gb & print stats
                uint64_t sum_bps = 0;
                std::string stats;
                for (auto& c: clients){
                    c.second.updateBps();
                    if (c.second.checkFourGigabytes(stats)){
                        ssize_t sz = send(c.first, stats.c_str(), stats.size(), MSG_NOSIGNAL);
                        if (sz != stats.size()){
                            std::cerr << c.first << " send() failed 1: " << strerror(errno) << std::endl;
                        }
                    }
                    stats = c.second.get_stats();
                    std::cout << stats << std::endl;
                    sum_bps += c.second.current_bps;
                }

                if (sum_bps > 0)
                    std::cout << "\t\ttotal:\t" << Stats::formatValue(sum_bps, "bps")<< std::endl;
                continue;
            }

            // stdin → socket
            if (fd == STDIN_FILENO && (evs & EPOLLIN)) {
                if (islisten){
                    d("server: stdin → sockets")
                    ssize_t n = read_from_stdin(buf, sizeof(buf));
                    d("readstdin " << n)
                    if (n > 0) {
                        for (auto c: clients){
                            d("send " << c.first)
                            if (send(c.first, buf, n, MSG_NOSIGNAL) != n) {
                                std::cerr << c.first << " send() failed 2: " << strerror(errno) << std::endl;
                                socket_closed = true;
                            }
                        }
                    } else if (n == 0) {
                        // EOF (Ctrl+D)
                        // чтобы другая сторона дочитала все
                        shutdown(sockfd, SHUT_WR);
                        stdin_closed = true;
                    } else {
                        perror("read stdin");
                        break;
                    }
                }else{
                    d("client: stdin → socket")
                    ssize_t n = read_from_stdin(buf, sizeof(buf));
                    d("cl read_from_stdin " << n)
                    if (n > 0) {
                        d("cl send " << n)
                        if (send(sockfd, buf, n, MSG_NOSIGNAL) != n) {
                            std::cerr << "send() failed 3: " << strerror(errno) << std::endl;
                            socket_closed = true;
                        }
                        d("cl af send " << n)
                    } else if (n == 0) {
                        d("cl eof " << n)
                        // EOF (Ctrl+D)
                        // чтобы другая сторона дочитала все
                        shutdown(sockfd, SHUT_WR);
                        stdin_closed = true;
                    } else {
                        perror("read stdin");
                        break;
                    }
                }
                continue;
            }

            // socket → stdout
            if (fd == sockfd && (evs & EPOLLIN)) {
                if (islisten){
                    d("server: get client socket ")
                        while (true) {
                        sockaddr_in client_addr{};
                        socklen_t addr_len = sizeof(client_addr);
                        int client_fd = accept4(sockfd,
                                                (sockaddr*)&client_addr,
                                                &addr_len,
                                                SOCK_NONBLOCK | SOCK_CLOEXEC);
                        // std::cout << "accept " << client_fd << std::endl;
                        if (client_fd == -1) {
                            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                                // Все подключения обработаны
                                // std::cout << "all accepted " << std::endl;
                                break;
                            } else {
                                perror("accept4");
                                socket_closed = true;
                                break;
                            }
                        }

                        // Добавление клиентского сокета в epoll
                        epoll_event client_event{};
                        client_event.events = EPOLLIN | EPOLLRDHUP;
                        client_event.data.fd = client_fd;
                        if (epoll_ctl(epfd, EPOLL_CTL_ADD, client_fd, &client_event) == -1) {
                            perror("epoll_ctl client");
                            close(client_fd);
                            continue;
                        }

                        char ip_str[INET_ADDRSTRLEN];
                        inet_ntop(AF_INET, &client_addr.sin_addr, ip_str, INET_ADDRSTRLEN);
                        uint16_t port = ntohs(client_addr.sin_port);
                        std::string ipstats = std::string(ip_str) + ":" + std::to_string(port);

                        Stats st;
                        st.ip = ipstats;
                        clients.emplace(client_fd, std::move(st));
                        std::cout << "Added socket: " << client_fd << " (" << ipstats << ")" << std::endl;
                        // addSocket(client_fd, ipstats);
                    }
                }else{
                    d("client: get data from socket")
                    ssize_t n = recv(sockfd, buf, sizeof(buf), 0);
                    if (n > 0) {
                        if (write_to_stdout(buf, n) != 0) {
                            std::cerr << "write to stdout failed" << std::endl;
                            break;
                        }
                    } else if (n == 0) {
                        // Пир закрыл соединение
                        socket_closed = true;
                    } else {
                        std::cerr << "recv() failed: " << strerror(errno) << std::endl;
                        socket_closed = true;
                    }
                }

            }

            auto client_sock = clients.find(fd);
            if (client_sock != clients.end()){

                ssize_t n = recv(client_sock->first, buf, sizeof(buf), 0);
                d("server: socket → stdout " << n)
                client_sock->second.addBytes(n);
                if (n > 0) {
                    if (write_to_stdout(buf, n) != 0) {
                        std::cerr << "write to stdout failed" << std::endl;
                        break;
                    }
                } else if (n == 0) {
                    // Пир закрыл соединение
                    socket_closed = true;
                } else {
                    std::cerr << "recv() failed: " << strerror(errno) << std::endl;
                    socket_closed = true;
                }
            }

        }
    }

    close(epfd);
}

// void listen_mode_epoll(int port)
// {
//     int listen_fd = create_socket(true, "0.0.0.0", port);
//     if (listen_fd < 0) return;

//     std::cout << "Listening on port " << port << "...\n";

//     bidirectional_relay(listen_fd, true);
// }

// void client_mode_epoll(std::string host, int port)
// {
//     int sockfd = create_socket(false, host, port);
//     if (sockfd < 0) return;

//     bidirectional_relay(sockfd, false);
// }


void listen_mode_epoll(int port)
{
    int listen_fd = create_socket(true, "0.0.0.0", port);
    if (listen_fd < 0) return;

    std::cout << "Listening on port " << port << "...\n";

    Epoll e(listen_fd, true);
    e.exec();
}

void client_mode_epoll(std::string host, int port)
{
    int sockfd = create_socket(false, host, port);
    if (sockfd < 0) return;

    Epoll e(sockfd, false);
    e.exec();
}

#endif // EPOLL_H
