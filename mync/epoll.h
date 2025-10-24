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

#define d(x) std::cout << x << std::endl;
const size_t BUF_SIZE = 1 * 1024 * 1024; // 1MB

class Epoll {
    int epfd;
    int sockfd;
    bool is_listen;
    int timerfd = -1;
    std::unordered_map<int, Stats> clients;
    time_t start_time;
    char buffer[BUF_SIZE];// 65536 65Kb
    bool stdin_closed = false;
    bool socket_closed = false;

    void setup_epoll() {
        epfd = epoll_create1(EPOLL_CLOEXEC);
        if (epfd == -1) throw std::runtime_error("epoll_create1");

        add_fd(STDIN_FILENO, EPOLLIN | EPOLLRDHUP);
        add_fd(sockfd, EPOLLIN | EPOLLRDHUP );//EPOLLET

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
        ssize_t n;
        n = recv(fd, buffer, sizeof(buffer), 0);
        if (n > 0) {
            clients[fd].addBytes(n);
            if (write_to_stdout(buffer, n) != 0) {
                throw std::runtime_error("write to stdout");
            }
        } else {
            remove_client(fd);
        }

        //for EPOLLET
        // d("before recv1")
        // ssize_t n;
        // while ((n = recv(fd, buffer, sizeof(buffer), 0)) > 0) {
        //     // d("recv " << n)
        //     clients[fd].addBytes(n);
        //     if (write_to_stdout(buffer, n) != 0) {
        //         throw std::runtime_error("write to stdout");
        //     }
        // }

        // if (n == 0) {
        //     // d("connection closed by peer")
        //     remove_client(fd);
        // } else  if (n < 0 && !(errno == EAGAIN || errno == EWOULDBLOCK)) {
        //     throw std::runtime_error("recv from socket");
        // }
    }

    void handle_socket_data() {
        if (is_listen) {
            accept_connections();
        } else {
            // handle_client_data
            ssize_t n;
            n = recv(sockfd, buffer, sizeof(buffer), 0);
            if (n > 0) {
                if (write_to_stdout(buffer, n) != 0) {
                    throw std::runtime_error("write to stdout");
                }
            } else if (n == 0) {
                socket_closed = true;
            } else {
                throw std::runtime_error("recv from socket");
            }

            // for EPOLLET
            // ssize_t n;
            // while ((n = recv(sockfd, buffer, sizeof(buffer), 0)) > 0) {
            //     if (write_to_stdout(buffer, n) != 0) {
            //         throw std::runtime_error("write to stdout");
            //     }
            // }

            // if (n == 0) {
            //     socket_closed = true;
            // } else  if (n < 0 && !(errno == EAGAIN || errno == EWOULDBLOCK)) {
            //     throw std::runtime_error("recv from socket");
            // }

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

            // Увеличение буфера отправки
            const int bufsize = BUF_SIZE;
            if (setsockopt(client_fd, SOL_SOCKET, SO_SNDBUF, &bufsize, sizeof(bufsize)) < 0) {
                perror("setsockopt SO_SNDBUF");
            }

            // Увеличение буфера приема
            if (setsockopt(client_fd, SOL_SOCKET, SO_RCVBUF, &bufsize, sizeof(bufsize)) < 0) {
                perror("setsockopt SO_RCVBUF");
            }

            add_fd(client_fd, EPOLLIN | EPOLLRDHUP );

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
            std::cout << "\t\ttotal:\t\t" << Stats::formatValue(total_bps, "bps") << std::endl;
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
                if (evs & (EPOLLHUP | EPOLLRDHUP | EPOLLERR)) { // EPOLLET
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
