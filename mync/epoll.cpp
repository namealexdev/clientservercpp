#include "epoll.h"
#include "utils.h"
#include <sys/timerfd.h>

Epoll::Epoll(int sock, bool listen, int show_timer_stats) :
    sockfd(sock), is_listen(listen), show_timer_stats(show_timer_stats)
{
    start_time = time(nullptr);
    setup_epoll();
}


Epoll::~Epoll()
{
    if (epfd >= 0) close(epfd);
    if (timerfd >= 0) close(timerfd);
    if (event_external_fd >= 0) close(event_external_fd);
}

void Epoll::exec()
{
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
                if (event_external_fd > 0 && events[i].data.fd == event_external_fd) {
                    // прочитать все eventfd
                    uint64_t val;
                    read(event_external_fd, &val, sizeof(val));
                    // добавить все pending_socks в epoll
                    std::lock_guard lock(mtx_pending_new_socks_);
                    while (!pending_new_socks_.empty()) {
                        auto data = pending_new_socks_.front(); pending_new_socks_.pop();
                        add_fd(data.first, EPOLLIN | EPOLLRDHUP);

                        size_clients++;
                        clients.emplace(data.first, std::move(data.second));

                        // epoll_event ev{};
                        // ev.events = EPOLLIN;
                        // ev.data.fd = fd;
                        // epoll_ctl(event_external_fd, EPOLL_CTL_ADD, fd, &ev);
                        // ++count_socks;
                    }
                }else
                    if (fd == STDIN_FILENO) handle_stdin();
                    else if (fd == timerfd) handle_timer();
                    else if (sockfd > 0 && fd == sockfd) handle_socket_data();
                    else if (clients.count(fd)) handle_client_data(fd);
            }
        }
    }
}

int Epoll::create_timer()
{
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

void Epoll::setup_epoll()
{
    epfd = epoll_create1(EPOLL_CLOEXEC);
    if (epfd == -1) throw std::runtime_error("epoll_create1");

    add_fd(STDIN_FILENO, EPOLLIN | EPOLLRDHUP);

    if (sockfd > 0){
        add_fd(sockfd, EPOLLIN | EPOLLRDHUP );//EPOLLET
    }

    event_external_fd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
    if (event_external_fd == -1) {
        throw std::runtime_error("eventfd");
    }
    add_fd(event_external_fd, EPOLLIN);

    // only for server and subserver
    if (is_listen || sockfd == -1) {
        timerfd = create_timer();
        if (timerfd > 0)
            add_fd(timerfd, EPOLLIN);
    }
}

bool Epoll::add_fd(int fd, uint32_t events)
{
    epoll_event ev{};
    ev.events = events;
    ev.data.fd = fd;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev) == -1) {
        // throw std::runtime_error("epoll_ctl add");
        std::cout << "fail epoll_ctl add" << std::endl;
        return false;
    }

    // не нужно для статистики
    // if (!(fd == STDIN_FILENO || event_external_fd)){
    // count_fd++;
    // }
    return true;
}

void Epoll::handle_stdin()
{
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

void Epoll::handle_client_data(int fd) {
    ssize_t n;
    n = recv(fd, buffer, sizeof(buffer), 0);
    if (n > 0) {
        // std::cout << "1handle_client_data " << n << std::endl;
        clients[fd].addBytes(n);
        // if (write_to_stdout(buffer, n) != 0) {
        //     throw std::runtime_error("write to stdout");
        // }
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

void Epoll::handle_socket_data()
{
    if (is_listen) {
        accept_connections();
    } else {
        // handle_client_data
        ssize_t n;
        // это не SubEpoll, тут не нужна статистика
        // std::cout << "2handle_socket_data " << n << std::endl;
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


void Epoll::accept_connections()
{
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
        // const int bufsize = BUF_SIZE;
        // if (setsockopt(client_fd, SOL_SOCKET, SO_SNDBUF, &bufsize, sizeof(bufsize)) < 0) {
        //     perror("setsockopt SO_SNDBUF");
        // }

        // // Увеличение буфера приема
        // if (setsockopt(client_fd, SOL_SOCKET, SO_RCVBUF, &bufsize, sizeof(bufsize)) < 0) {
        //     perror("setsockopt SO_RCVBUF");
        // }

        char ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, ip_str, INET_ADDRSTRLEN);
        Stats st;
        st.ip = std::string(ip_str) + ":" + std::to_string(ntohs(client_addr.sin_port));

        // тут надо распределять это все по потокам
        if (!balance_socket(client_fd, st)){
            if (!add_fd(client_fd, EPOLLIN | EPOLLRDHUP )){
                close(client_fd);
            }else{
                clients.emplace(client_fd, std::move(st));
                size_clients++;
                std::cout << "Added socket: " << client_fd << " (" << clients[client_fd].ip << ")" << std::endl;
            }

        }

    }
}

// collect stats (and send stats to socket)
void Epoll::handle_timer()
{
    uint64_t expirations;
    if (read(timerfd, &expirations, sizeof(expirations)) != sizeof(expirations)) return;

    if (show_timer_stats){
        time_t now = time(nullptr);
        std::cout << "======== Uptime: " << (now - start_time) << " s\n";
        show_shared_stats();
    }

    // uint64_t total_bps = 0;
    for (auto& [fd, stats] : clients) {
        stats.updateBps();
        std::string stats_msg;
        if (stats.checkFourGigabytes(stats_msg)) {
            send(fd, stats_msg.c_str(), stats_msg.size(), MSG_NOSIGNAL);
        }
        // if (show_timer_stats){
        //     std::cout << stats.get_stats() << std::endl;
        //     total_bps += stats.current_bps;
        // }
    }

    // if (show_timer_stats && total_bps > 0) {
    //     std::cout << "\t\ttotal:\t\t" << Stats::formatValue(total_bps, "bps") << std::endl;
    // }
}

void Epoll::remove_client(int fd) {
    // count_fd--;
    epoll_ctl(epfd, EPOLL_CTL_DEL, fd, nullptr);
    clients.erase(fd);
    size_clients--;
    close(fd);
}

void Epoll::push_external_socket(int client_fd, const Stats &st) {
    {
        std::lock_guard lock(mtx_pending_new_socks_);
        pending_new_socks_.push(std::make_pair(client_fd, std::move(st)));
    }
    uint64_t one = 1;
    write(event_external_fd, &one, sizeof(one)); // разбудить epoll
}

void listen_mode_epoll(int port)
{
    int listen_fd = create_socket(true, "0.0.0.0", port);
    if (listen_fd < 0) return;

    std::cout << "Listening on port " << port << "...\n";

    MainEpoll e(listen_fd, true);
    e.exec();
}

void client_mode_epoll(std::string host, int port)
{
    int sockfd = create_socket(false, host, port);

    if (sockfd < 0) return;

    Epoll e(sockfd, false, false);
    e.exec();
}
