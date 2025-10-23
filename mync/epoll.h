#ifndef EPOLL_H
#define EPOLL_H

#include "utils.h"
#include <sys/epoll.h>
#include <unordered_map>
#include <unordered_set>
#include "stats.h"

const int timer_timeout_secs = 1;

// Двунаправленная передача: stdin ↔ sockfd
class Epoll{
    int epfd_ = -1;

    char buf[65536];
    bool stdin_closed = false;
    bool socket_closed = false;
    bool need_stop = false;

    const int MAX_EVENTS = 4;// 4 fd: 1 консоль, 1 accept(можно много), 1 client, 1 timer?

    bool is_listen_sock_ = false;
    int main_sock_;
    std::unordered_map<int, Stats> sockfds_;// больше искать надо
    // std::unordered_set<int, Stats> sockfds_;
public:
    Epoll(int sockfd, bool is_listen_sock):
        is_listen_sock_(is_listen_sock),
        main_sock_(sockfd)
    {
        epfd_ = epoll_create1(EPOLL_CLOEXEC);
        if (epfd_ == -1) {
            perror("epoll_create1");
            return;
        }

        struct epoll_event ev;
        ev.events = EPOLLIN;
        ev.data.fd = STDIN_FILENO;// консоль
        std::cout << "add stdin " << STDIN_FILENO << std::endl;
        if (epoll_ctl(epfd_, EPOLL_CTL_ADD, STDIN_FILENO, &ev) == -1) {
            perror("epoll_ctl stdin");
        }else{
            std::cout<<"ok epoll_ctl stdin" << std::endl;
        }

        string ipstats = is_listen_sock ? "main listen" : "main client";
        ev.events = EPOLLIN | EPOLLRDHUP;
        ev.data.fd = sockfd;
        std::cout << "add main " << sockfd << std::endl;
        epoll_ctl(epfd_, EPOLL_CTL_ADD, sockfd, &ev);
        // addSocket(sockfd, ipstats);
    }
    ~Epoll(){
        if (epfd_ != -1) {
            close(epfd_);
        }
        close(main_sock_);
        for (auto& s: sockfds_){
            close(s.first);
        }

    }

    void addSocket(int sockfd, string ipstats){
        if (sockfd <= 0) return;

        struct epoll_event ev;
        ev.events = EPOLLIN | EPOLLRDHUP;// server
        if (!is_listen_sock_){// clients
            // ev.events |= EPOLLET | EPOLLRDHUP | EPOLLHUP | EPOLLERR;
        }
        ev.data.fd = sockfd;
        std::cout << "add " << sockfd << std::endl;
        if (epoll_ctl(epfd_, EPOLL_CTL_ADD, sockfd, &ev) == -1) {
            perror("epoll_ctl add socket " );
            return;
        }
        if (sockfd != main_sock_){
            Stats st;
            st.ip = ipstats;
            sockfds_.emplace(sockfd, std::move(st));
            std::cout << "Added socket: " << sockfd << " (" << ipstats << ")" << std::endl;
        }
    }

    void removeSocket(int sockfd) {
        if (sockfd <= 0) return;

        epoll_ctl(epfd_, EPOLL_CTL_DEL, sockfd, nullptr);

        close(sockfd);

        if (sockfd != main_sock_) {
            auto it = sockfds_.find(sockfd);
            if (it != sockfds_.end()) {
                std::cout << "Removed socket: " << sockfd << " (" << it->second.ip << ")" << std::endl;
                sockfds_.erase(it);
            }
        }
    }

    // stdin → socket
    void stdin_handle(int client_sock){
        ssize_t n = read_from_stdin(buf, sizeof(buf));
        if (n > 0) {
            if (is_listen_sock_){
                for (auto&s : sockfds_){
                    ssize_t sent = send(s.first, buf, n, MSG_NOSIGNAL);
                    if (sent != n) {
                        std::cerr << s.first << " send() failed: " << strerror(errno) << " " << sent << " of " << n << std::endl;
                        // socket_closed = true;
                        // removeSocket(client_sock);
                    }
                }
            }else{
                ssize_t sent = send(client_sock, buf, n, MSG_NOSIGNAL);
                if (sent != n) {
                    std::cerr << "send() failed: " << strerror(errno) << " " << sent << " of " << n << std::endl;
                    // socket_closed = true;
                    // removeSocket(client_sock);
                }
            }

        } else if (n == 0) {
            // EOF (Ctrl+D)
            // чтобы другая сторона дочитала все
            if (!is_listen_sock_){
                shutdown(client_sock, SHUT_WR);
            }

            stdin_closed = true;
            std::cout << "Stdin closed, shutdown WR for socket " << client_sock << std::endl;
        } else {
            perror("read stdin");
            need_stop = true;
            return;
        }
    }

    // socket → stdout
    // void recv_handle(int sockfd){
    //     ssize_t n = recv(sockfd, buf, sizeof(buf), 0);
    //     if (n > 0) {
    //         if (write_to_stdout(buf, n) != 0) {
    //             std::cerr << "write to stdout failed" << std::endl;
    //             need_stop = true;
    //             return;
    //         }
    //     } else if (n == 0) {
    //         // Пир закрыл соединение
    //         socket_closed = true;
    //         removeSocket(sockfd);
    //     } else {
    //         std::cerr << "recv() failed: " << strerror(errno) << std::endl;
    //         socket_closed = true;
    //     }
    // }
    void recv_handle(int sockfd) {

        // В ET режиме читаем все доступные данные
        while (true) {
            ssize_t n = recv(sockfd, buf, sizeof(buf), 0);
            if (n > 0) {
                if (write_to_stdout(buf, n) != 0) {
                    std::cerr << "write to stdout failed" << std::endl;
                    need_stop = true;
                    return;
                }
            } else if (n == 0) {
                // Пир закрыл соединение
                std::cout << "Connection closed by peer on socket " << sockfd << std::endl;
                removeSocket(sockfd);
                break;
            } else {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    // Все данные прочитаны в ET режиме
                    break;
                } else {
                    std::cerr << "recv() failed on socket " << sockfd << ": " << strerror(errno) << std::endl;
                    removeSocket(sockfd);
                    break;
                }
            }
        }
    }

    void accept_handle(){
        // std::cout << "accept_handle" << std::endl;
        // В ET режиме принимаем все доступные соединения
        while (true) {
            sockaddr_in client_addr{};
            socklen_t addr_len = sizeof(client_addr);
            int client_fd = accept4(main_sock_,
                                    (sockaddr*)&client_addr,
                                    &addr_len,
                                    SOCK_NONBLOCK | SOCK_CLOEXEC);
            // std::cout << "accept " << client_fd << std::endl;
            if (client_fd == -1) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    // Все подключения обработаны
                    std::cout << "all accepted " << std::endl;
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
            if (epoll_ctl(epfd_, EPOLL_CTL_ADD, client_fd, &client_event) == -1) {
                perror("epoll_ctl client");
                close(client_fd);
                continue;
            }

            char ip_str[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &client_addr.sin_addr, ip_str, INET_ADDRSTRLEN);
            uint16_t port = ntohs(client_addr.sin_port);
            std::string ipstats = std::string(ip_str) + ":" + std::to_string(port);

            std::cout << "add client " << ipstats << std::endl;
            addSocket(client_fd, ipstats);
        }
    }

    void exec() {
        // while (!need_stop && (!stdin_closed || !socket_closed)) {
        while (!stdin_closed || !socket_closed) {

            struct epoll_event events[MAX_EVENTS];
            int nfds = epoll_wait(epfd_, events, MAX_EVENTS, -1);// блок
            // std::cout << "epoll " << nfds << std::endl;
            if (nfds == -1) {
                if (errno == EINTR) continue;
                perror("epoll_wait");
                need_stop = true;
                break;
            }

            for (int i = 0; i < nfds; ++i) {
                int fd = events[i].data.fd;
                uint32_t evs = events[i].events;


                // stdin → socket
                if (fd == STDIN_FILENO && (evs & EPOLLIN)) {
                    if (is_listen_sock_){
                        ssize_t n = read_from_stdin(buf, sizeof(buf));
                        if (n > 0) {
                            for (auto& s: sockfds_){
                                if (send(s.first, buf, n, MSG_NOSIGNAL) != n) {
                                    std::cerr << "send() failed: " << strerror(errno) << std::endl;
                                    // socket_closed = true;
                                }
                            }

                        } else if (n == 0) {
                            // EOF (Ctrl+D)
                            // чтобы другая сторона дочитала все
                            shutdown(main_sock_, SHUT_WR);
                            stdin_closed = true;
                        } else {
                            perror("read stdin");
                            break;
                        }
                    }else{
                        ssize_t n = read_from_stdin(buf, sizeof(buf));
                        if (n > 0) {
                            if (send(main_sock_, buf, n, MSG_NOSIGNAL) != n) {
                                std::cerr << "send() failed: " << strerror(errno) << std::endl;
                                socket_closed = true;
                            }
                        } else if (n == 0) {
                            // EOF (Ctrl+D)
                            // чтобы другая сторона дочитала все
                            shutdown(main_sock_, SHUT_WR);
                            stdin_closed = true;
                        } else {
                            perror("read stdin");
                            break;
                        }
                    }

                }

                // socket → stdout
                if (fd == main_sock_ && (evs & EPOLLIN)) {
                    if (is_listen_sock_){
                        accept_handle();
                    }else{
                        // std::cout << "recv and stdout";
                        ssize_t n = recv(main_sock_, buf, sizeof(buf), 0);
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

                /*
                // std::cout << i << " epoll " << fd << " " << evs << std::endl;

                // Проверка на закрытие/ошибку
                //HUP-закрыт ERR-ошибка RDHUP-удаленно закрыл запись (узнаем без чтения)
                if (evs & (EPOLLHUP | EPOLLRDHUP | EPOLLERR)) {
                    // std::cout << "Error/hangup on fd " << fd << std::endl;
                    if (fd == main_sock_) {
                        socket_closed = true;
                        need_stop = true;
                    } else {
                        removeSocket(fd);
                    }
                    continue;
                }

                // stdin → socket
                if (fd == STDIN_FILENO && (evs & EPOLLIN)) {
                    std::cout << "stdin → socket "<< is_listen_sock_ << std::endl;
                    if (is_listen_sock_){
                        // server mode - need write to all clients ???
                        stdin_handle(0);
                        // for (auto s: sockfds_){

                        //     if (need_stop) break;
                        // }
                    }else{
                        // need send
                        stdin_handle(main_sock_);
                        // if (need_stop) break;
                    }
                    continue;
                }

                // socket → stdout
                // надо ли проверять при listen на && (evs & EPOLLIN) - работает
                if (fd == main_sock_ && (evs & EPOLLIN)) {
                    std::cout << "socket → stdout "<< is_listen_sock_ << std::endl;
                    if (is_listen_sock_){
                        // server mode - accept
                        accept_handle();
                    }else{
                        // client mode - recv -> stdout

                        // recv_handle(main_sock_);
                        // if (need_stop) break;

                        ssize_t n = recv(main_sock_, buf, sizeof(buf), 0);
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
                    continue;
                }
*/
                // clients -> stdout
                auto client_sock = sockfds_.find(fd);
                if (client_sock != sockfds_.end()){
                    std::cout << "clients -> stdout "<< is_listen_sock_ << std::endl;

                    // recv_handle(client_sock->first);
                    // if (need_stop) break;
                    ssize_t n = recv(client_sock->first, buf, sizeof(buf), 0);
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

                    continue;
                }

            }// end for nfds

        }// end while

        if (need_stop){
            close(epfd_);
            epfd_ = -1;
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

    char buf[65536];
    bool stdin_closed = false;
    bool socket_closed = false;
    std::unordered_map<int, Stats> clients;

    while (!stdin_closed || !socket_closed) {
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

            d(i <<" epoll " << fd << " " << evs)

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
                                std::cerr << c.first << " send() failed: " << strerror(errno) << std::endl;
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
                            std::cerr << "send() failed: " << strerror(errno) << std::endl;
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
                d("server: socket → stdout")
                ssize_t n = recv(client_sock->first, buf, sizeof(buf), 0);
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

void listen_mode_epoll(int port)
{
    int listen_fd = create_socket(true, "0.0.0.0", port);
    if (listen_fd < 0) return;

    std::cout << "Listening on port " << port << "...\n";

    bidirectional_relay(listen_fd, true);
}

void client_mode_epoll(std::string host, int port)
{
    int sockfd = create_socket(false, host, port);
    if (sockfd < 0) return;

    bidirectional_relay(sockfd, false);
}


// void listen_mode_epoll(int port)
// {
//     int listen_fd = create_socket(true, "0.0.0.0", port);
//     if (listen_fd < 0) return;

//     std::cout << "Listening on port " << port << "...\n";

//     while(1){
//         int client_fd = accept4(listen_fd, nullptr, nullptr, SOCK_CLOEXEC);
//         // close(listen_fd);

//         if (client_fd < 0) {
//             std::cerr << "accept() failed: " << strerror(errno) << std::endl;
//             return;
//         }


//         bidirectional_relay_old(client_fd);
//         close(client_fd);

//     }

// }

// void client_mode_epoll(std::string host, int port)
// {
//     int sockfd = create_socket(false, host, port);
//     if (sockfd < 0) return;

//     bidirectional_relay_old(sockfd);
//     close(sockfd);
// }

// void listen_mode_epoll(int port)
// {
//     int listen_fd = create_socket(true, "0.0.0.0", port);
//     if (listen_fd < 0) return;

//     std::cout << "Listening on port " << port << "...\n";

//     Epoll e(listen_fd, true);
//     e.exec();
// }

// void client_mode_epoll(std::string host, int port)
// {
//     int sockfd = create_socket(false, host, port);
//     if (sockfd < 0) return;

//     Epoll e(sockfd, false);
//     e.exec();
// }

#endif // EPOLL_H
