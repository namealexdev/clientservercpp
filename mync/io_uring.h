#ifndef IO_URING_H
#define IO_URING_H
#include "utils.h"
#include <liburing.h>

static void bidirectional_relay_io_uring(int sockfd) {
    const size_t BUF_SIZE = 65536;
    char buf_stdin[BUF_SIZE];   // буфер для stdin → socket
    char buf_socket[BUF_SIZE];  // буфер для socket → stdout

    struct io_uring ring;
    // одновременные операции, которые можно положить в очередь - 32
    // зависит от ожидаемого кол-ва системных вызовов
    if (io_uring_queue_init(32, &ring, 0) != 0) {
        perror("io_uring_queue_init");
        return;
    }

    // Флаги завершения
    bool stdin_closed = false;
    bool socket_closed = false;
    //какие операции сейчас выполняются в ядре
    bool pending_stdin_read = false;
    bool pending_socket_read = false;

    // Запускаем первые операции чтения
    if (!stdin_closed) {
        // получение Submission Queue - очередь на исполнение - Кольцевой буфер операций для отправки в ядро
        struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
        io_uring_prep_read(sqe, STDIN_FILENO, buf_stdin, BUF_SIZE, 0);
        sqe->user_data = 1; // 1 = stdin
        pending_stdin_read = true;
    }

    if (!socket_closed) {
        struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
        io_uring_prep_recv(sqe, sockfd, buf_socket, BUF_SIZE, 0);
        sqe->user_data = 2; // 2 = socket
        pending_socket_read = true;
    }

    io_uring_submit(&ring);// отправка операций на ядро

    while (!stdin_closed || !socket_closed || pending_stdin_read || pending_socket_read) {
        // cqe - completion queue - очередь завершения
        struct io_uring_cqe *cqe;
        // ожидание любой завершенной операции
        int ret = io_uring_wait_cqe(&ring, &cqe);
        if (ret != 0) {
            if (ret == -EINTR) continue;
            perror("io_uring_wait_cqe");
            break;
        }

        uint64_t user_data = cqe->user_data;
        int res = cqe->res;
        // помечаем завершенную операцию как обработанную
        io_uring_cqe_seen(&ring, cqe);

        // stdin → socket
        if (user_data == 1) {
            pending_stdin_read = false;
            if (res > 0) {
                // Отправляем в сокет
                ssize_t sent = send(sockfd, buf_stdin, res, MSG_NOSIGNAL);
                if (sent != res) {
                    std::cerr << "send() failed: " << strerror(errno) << std::endl;
                    socket_closed = true;
                }
            } else if (res == 0) {
                // EOF из stdin
                shutdown(sockfd, SHUT_WR);
                stdin_closed = true;
            } else {
                perror("stdin read error");
                break;
            }

            // Запрашиваем следующее чтение из stdin, если ещё не закрыт
            if (!stdin_closed) {
                struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
                if (sqe == NULL){
                    // очередь полна!
                    io_uring_submit(&ring);  // Принудительный системный вызов
                    sqe = io_uring_get_sqe(&ring); // Теперь место есть
                }
                io_uring_prep_read(sqe, STDIN_FILENO, buf_stdin, BUF_SIZE, 0);
                sqe->user_data = 1;
                pending_stdin_read = true;
                io_uring_submit(&ring);// отправка операций на ядро
            }

        }
        // socket → stdout
        else if (user_data == 2) {
            pending_socket_read = false;
            if (res > 0) {
                if (write(STDOUT_FILENO, buf_socket, res) != res) {
                    std::cerr << "write to stdout failed" << std::endl;
                    break;
                }
            } else if (res == 0) {
                // Пир закрыл соединение
                socket_closed = true;
            } else {
                std::cerr << "recv() failed: " << strerror(errno) << std::endl;
                socket_closed = true;
            }

            // Запрашиваем следующее чтение из сокета, если ещё не закрыт
            if (!socket_closed) {
                struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
                io_uring_prep_recv(sqe, sockfd, buf_socket, BUF_SIZE, 0);
                sqe->user_data = 2;
                pending_socket_read = true;
                io_uring_submit(&ring);
            }
        }
    }

    io_uring_queue_exit(&ring);
}

void listen_mode_iouring(int port)
{
    int listen_fd = create_socket(true, "0.0.0.0", port);
    if (listen_fd < 0) return;

    std::cout << "Listening on port " << port << "...\n";

    int client_fd = accept4(listen_fd, nullptr, nullptr, SOCK_CLOEXEC);
    close(listen_fd);
    if (client_fd < 0) {
        std::cerr << "accept() failed: " << strerror(errno) << std::endl;
        return;
    }

    bidirectional_relay_io_uring(client_fd);
    close(client_fd);
}

void client_mode_iouring(std::string host, int port)
{
    int sockfd = create_socket(false, host, port);
    if (sockfd < 0) return;
    bidirectional_relay_io_uring(sockfd);
    close(sockfd);
}

#endif // IO_URING_H
