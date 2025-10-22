#ifndef EPOLL_H
#define EPOLL_H

#include "utils.h"
#include <sys/epoll.h>

// Двунаправленная передача: stdin ↔ sockfd
class Server{
    int epfd_;

    char buf[65536];
    bool stdin_closed = false;
    bool socket_closed = false;
    bool need_stop = false;

public:
    Server(int sockfd){
        epfd_ = epoll_create1(EPOLL_CLOEXEC);
        if (epfd_ == -1) {
            perror("epoll_create1");
            return;
        }

        struct epoll_event ev;
        ev.events = EPOLLIN | EPOLLRDHUP;// готов к чтению и закрыл соединение
        ev.data.fd = STDIN_FILENO;// консоль
        epoll_ctl(epfd_, EPOLL_CTL_ADD, STDIN_FILENO, &ev);
    }

    void addSocket(int sockfd){
        if (sockfd <= 0) return;
        struct epoll_event ev;
        ev.events = EPOLLIN | EPOLLRDHUP;
        ev.data.fd = sockfd;// сокет
        epoll_ctl(epfd_, EPOLL_CTL_ADD, sockfd, &ev);
    }

    void bidirectional_relay() {

    }

};

static void bidirectional_relay(int sockfd) {
    static bool isinit = false;
    static int epfd;
    const int MAX_EVENTS = 2;// 2 fd консоль и сокет

    if (!isinit){
        epfd = epoll_create1(EPOLL_CLOEXEC);
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
        isinit = true;
    }

    // if (sockfd)

    char buf[65536];
    bool stdin_closed = false;
    bool socket_closed = false;
    bool need_stop = false;

    while (!stdin_closed || !socket_closed) {
        static struct epoll_event events[MAX_EVENTS];
        int nfds = epoll_wait(epfd, events, MAX_EVENTS, -1);// блок
        if (nfds == -1) {
            if (errno == EINTR) continue;
            perror("epoll_wait");
            need_stop = true;
            break;
        }

        for (int i = 0; i < nfds; ++i) {
            int fd = events[i].data.fd;
            uint32_t evs = events[i].events;

            // Проверка на закрытие/ошибку
            //HUP-закрыт ERR-ошибка RDHUP-удаленно закрыл запись (узнаем без чтения)
            if (evs & (EPOLLHUP | EPOLLRDHUP | EPOLLERR)) {
                if (fd == sockfd) socket_closed = true;
                continue;
            }

            // stdin → socket
            if (fd == STDIN_FILENO && (evs & EPOLLIN)) {
                ssize_t n = read_from_stdin(buf, sizeof(buf));
                if (n > 0) {
                    if (send(sockfd, buf, n, MSG_NOSIGNAL) != n) {
                        std::cerr << "send() failed: " << strerror(errno) << std::endl;
                        socket_closed = true;
                    }
                } else if (n == 0) {
                    // EOF (Ctrl+D)
                    // чтобы другая сторона дочитала все
                    shutdown(sockfd, SHUT_WR);
                    stdin_closed = true;
                } else {
                    perror("read stdin");
                    need_stop = true;
                    break;
                }
            }

            // socket → stdout
            if (fd == sockfd && (evs & EPOLLIN)) {
                ssize_t n = recv(sockfd, buf, sizeof(buf), 0);
                if (n > 0) {
                    if (write_to_stdout(buf, n) != 0) {
                        std::cerr << "write to stdout failed" << std::endl;
                        need_stop = true;
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

        break;
    }

    if (need_stop){
        close(epfd);
        epfd = -1;
    }
    // close(epfd);
}

#include <chrono>
#include <sys/time.h>
#include <iomanip>
void listen_mode_epoll(int port)
{
    int listen_fd = create_socket(true, "0.0.0.0", port);
    if (listen_fd < 0) return;

    std::cout << "Listening on port " << port << "...\n";

    auto start = std::chrono::steady_clock::now();
    int client_fd;
    while(1){
        // std::cout << "befaccept " << listen_fd << std::endl;
        client_fd = accept4(listen_fd, nullptr, nullptr, SOCK_NONBLOCK);
        // close(listen_fd);
        // std::cout << "accept " << client_fd << " " << errno << std::endl;

        // if (client_fd < 0) {
        //     if (errno == EAGAIN || errno == EWOULDBLOCK) {
        //         std::cout << "No pending connections (EAGAIN), sleeping a bit...\n";
        //         // НЕ возвращайтесь! Это нормально.
        //         // Но лучше использовать epoll/io_uring вместо sleep!
        //         // sleep(2); // 10ms — временная заглушка
        //         continue;
        //     } else {
        //         std::cerr << "accept() failed: " << strerror(errno) << std::endl;
        //         return;
        //     }
        // }
    }
        // bidirectional_relay(client_fd);


        // auto end = std::chrono::steady_clock::now();
        // auto duration = std::chrono::duration_cast<std::chrono::seconds>(end - start);
        // std::cout << "check stats" << std::endl;
        // if (duration.count() == 1){
        //     //showstats
        //     struct timeval tv;
        //     gettimeofday(&tv, NULL); // Получаем текущее время в секундах и микросекундах

        //     // Преобразуем секунды в структуру tm
        //     time_t now_sec = tv.tv_sec;
        //     struct tm *now_tm = localtime(&now_sec);

        //     // Используем put_time и выводим микросекунды
        //     std::cout << std::put_time(now_tm, "%H:%M:%S") << "." << std::setfill('0') << std::setw(6) << tv.tv_usec << std::endl;
        // }

        // close(client_fd);



    // }

}

void client_mode_epoll(std::string host, int port)
{
    int sockfd = create_socket(false, host, port);
    if (sockfd < 0) return;

    bidirectional_relay(sockfd);
    close(sockfd);
}

#endif // EPOLL_H
