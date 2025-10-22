#ifndef EPOLL_H
#define EPOLL_H

#include "utils.h"
#include <sys/epoll.h>
#include <thread>

// Двунаправленная передача: stdin ↔ sockfd
static void bidirectional_relay(int sockfd) {
    const int MAX_EVENTS = 2;// 2 fd консоль и сокет
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
                    break;
                }
            }

            // socket → stdout
            if (fd == sockfd && (evs & EPOLLIN)) {
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
    }

    close(epfd);
}

void listen_mode_epoll(int port)
{
    int listen_fd = create_socket(true, "0.0.0.0", port);
    if (listen_fd < 0) return;

    std::cout << "Listening on port " << port << "...\n";

    while(1){
        int client_fd = accept4(listen_fd, nullptr, nullptr, SOCK_CLOEXEC);
        // close(listen_fd);

        if (client_fd < 0) {
            std::cerr << "accept() failed: " << strerror(errno) << std::endl;
            return;
        }

        std::thread([=]() {
            bidirectional_relay(client_fd);
            close(client_fd);
        }).detach();
    }

}

void client_mode_epoll(std::string host, int port)
{
    int sockfd = create_socket(false, host, port);
    if (sockfd < 0) return;

    bidirectional_relay(sockfd);
    close(sockfd);
}

#endif // EPOLL_H
