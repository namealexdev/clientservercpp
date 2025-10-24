#ifndef IO_URING_H
#define IO_URING_H
#include "utils.h"
#include <liburing.h>
#include <sys/timerfd.h>
#include <unordered_map>
#include "stats.h"
/*
static void bidirectional_relay_io_uring(int sockfd, bool islisten) {
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

    while(1){
        int client_fd = accept4(listen_fd, nullptr, nullptr, SOCK_NONBLOCK);
        // close(listen_fd);
        if (client_fd < 0) {
            std::cerr << "accept() failed: " << strerror(errno) << std::endl;
            return;
        }

        bidirectional_relay_io_uring(listen_fd, true);
    }

    // close(client_fd);
}

void client_mode_iouring(std::string host, int port)
{
    int sockfd = create_socket(false, host, port);
    if (sockfd < 0) return;
    bidirectional_relay_io_uring(sockfd, false);
    // close(sockfd);
}
*/
class UringBidirectionalRelay {
    int sockfd;
    bool is_listen;
    struct io_uring ring;
    std::unordered_map<int, Stats> clients;
    time_t start_time;
    bool stdin_closed = false;
    bool socket_closed = false;
    int timerfd = -1;

    enum class OpType {
        STDIN_READ = 1,
        SOCKET_READ = 2,
        ACCEPT = 3,
        TIMER_READ = 4,
        CLIENT_READ = 5
    };

    struct Request {
        OpType type;
        int fd;
        char* buffer;
        size_t buffer_size;
        // Для accept храним информацию о клиенте
        sockaddr_in* client_addr = nullptr;
        socklen_t* addr_len = nullptr;
    };

    void setup_uring() {
        if (io_uring_queue_init(64, &ring, 0) < 0) {
            throw std::runtime_error("io_uring_queue_init failed");
        }

        submit_stdin_read();

        if (is_listen) {
            submit_accept();
            setup_timer();
        } else {
            submit_socket_read();
        }

        io_uring_submit(&ring);
    }

    void submit_stdin_read() {
        if (stdin_closed) return;

        struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
        if (!sqe) {
            io_uring_submit(&ring);
            sqe = io_uring_get_sqe(&ring);
            if (!sqe) return;
        }

        auto *req = new Request{OpType::STDIN_READ, STDIN_FILENO, new char[65536], 65536};
        io_uring_prep_read(sqe, STDIN_FILENO, req->buffer, req->buffer_size, 0);
        io_uring_sqe_set_data(sqe, req);
    }

    void submit_socket_read() {
        if (socket_closed) return;

        struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
        if (!sqe) {
            io_uring_submit(&ring);
            sqe = io_uring_get_sqe(&ring);
            if (!sqe) return;
        }

        auto *req = new Request{OpType::SOCKET_READ, sockfd, new char[65536], 65536};
        io_uring_prep_recv(sqe, sockfd, req->buffer, req->buffer_size, 0);
        io_uring_sqe_set_data(sqe, req);
    }

    void submit_accept() {
        if (socket_closed) return;

        struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
        if (!sqe) {
            io_uring_submit(&ring);
            sqe = io_uring_get_sqe(&ring);
            if (!sqe) return;
        }

        auto *req = new Request{OpType::ACCEPT, sockfd, nullptr, 0};
        req->client_addr = new sockaddr_in;
        req->addr_len = new socklen_t(sizeof(sockaddr_in));

        io_uring_prep_accept(sqe, sockfd, (sockaddr*)req->client_addr, req->addr_len, SOCK_NONBLOCK | SOCK_CLOEXEC);
        io_uring_sqe_set_data(sqe, req);
    }

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
    void setup_timer() {
        timerfd = create_timer();
        if (timerfd < 0) return;

        struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
        if (!sqe) {
            io_uring_submit(&ring);
            sqe = io_uring_get_sqe(&ring);
            if (!sqe) return;
        }

        auto *req = new Request{OpType::TIMER_READ, timerfd, new char[sizeof(uint64_t)], sizeof(uint64_t)};
        io_uring_prep_read(sqe, timerfd, req->buffer, req->buffer_size, 0);
        io_uring_sqe_set_data(sqe, req);
    }

    void submit_client_read(int client_fd) {
        if (clients.find(client_fd) == clients.end()) return;

        struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
        if (!sqe) {
            io_uring_submit(&ring);
            sqe = io_uring_get_sqe(&ring);
            if (!sqe) return;
        }

        auto *req = new Request{OpType::CLIENT_READ, client_fd, new char[65536], 65536};
        io_uring_prep_recv(sqe, client_fd, req->buffer, req->buffer_size, 0);
        io_uring_sqe_set_data(sqe, req);
    }

    void handle_stdin_read(Request* req, int res) {
        if (res > 0) {
            if (is_listen) {
                // Server: broadcast to all clients
                for (auto& client : clients) {
                    if (send(client.first, req->buffer, res, MSG_NOSIGNAL) != res) {
                        std::cerr << client.first << " send() failed: " << strerror(errno) << std::endl;
                        // Не удаляем клиента здесь - это сделаем при следующем чтении
                    }
                }
            } else {
                // Client: send to server
                if (send(sockfd, req->buffer, res, MSG_NOSIGNAL) != res) {
                    std::cerr << "send() failed: " << strerror(errno) << std::endl;
                    socket_closed = true;
                }
            }
        } else if (res == 0) {
            // EOF (Ctrl+D)
            if (is_listen) {
                for (auto& client : clients) {
                    shutdown(client.first, SHUT_WR);
                }
            } else {
                shutdown(sockfd, SHUT_WR);
            }
            stdin_closed = true;
        } else {
            std::cerr << "stdin read error: " << strerror(-res) << std::endl;
            stdin_closed = true;
        }

        delete[] req->buffer;
        delete req;

        if (!stdin_closed) {
            submit_stdin_read();
            io_uring_submit(&ring);
        }
    }

    void handle_socket_read(Request* req, int res) {
        if (res > 0) {
            if (write_to_stdout(req->buffer, res) != 0) {
                std::cerr << "write to stdout failed" << std::endl;
            }
        } else if (res == 0) {
            socket_closed = true;
        } else {
            std::cerr << "socket read error: " << strerror(-res) << std::endl;
            socket_closed = true;
        }

        delete[] req->buffer;
        delete req;

        if (!socket_closed) {
            submit_socket_read();
            io_uring_submit(&ring);
        }
    }

    void handle_accept(Request* req, int res) {
        if (res >= 0) {
            int client_fd = res;

            // Get client info from the stored address
            char ip_str[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &req->client_addr->sin_addr, ip_str, INET_ADDRSTRLEN);
            uint16_t port = ntohs(req->client_addr->sin_port);
            std::string ipstats = std::string(ip_str) + ":" + std::to_string(port);
            Stats st;
            st.ip = ipstats;
            clients.emplace(client_fd, std::move(st));
            std::cout << "Added socket: " << client_fd << " (" << ipstats << ")" << std::endl;

            submit_client_read(client_fd);
        } else {
            std::cerr << "accept error: " << strerror(-res) << std::endl;
        }

        // Cleanup and resubmit
        delete req->client_addr;
        delete req->addr_len;
        delete req;

        if (!socket_closed) {
            submit_accept();
            io_uring_submit(&ring);
        }
    }

    void handle_timer_read(Request* req, int res) {
        if (res == sizeof(uint64_t)) {
            time_t now = time(nullptr);
            std::cout << "======== Uptime: " << (now - start_time) << " s clients: " << clients.size() << "\n";

            uint64_t total_bps = 0;
            for (auto& [fd, stats] : clients) {
                stats.updateBps();
                std::string stats_msg;
                if (stats.checkFourGigabytes(stats_msg)) {
                    if (send(fd, stats_msg.c_str(), stats_msg.size(), MSG_NOSIGNAL) != static_cast<ssize_t>(stats_msg.size())) {
                        std::cerr << fd << " send stats failed: " << strerror(errno) << std::endl;
                    }
                }
                std::cout << stats.get_stats() << std::endl;
                total_bps += stats.current_bps;
            }

            if (total_bps > 0) {
                std::cout << "\t\ttotal:\t" << Stats::formatValue(total_bps, "bps") << std::endl;
            }
        }

        // Resubmit timer
        delete[] req->buffer;
        delete req;

        if (timerfd >= 0) {
            struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
            if (sqe) {
                auto *new_req = new Request{OpType::TIMER_READ, timerfd, new char[sizeof(uint64_t)], sizeof(uint64_t)};
                io_uring_prep_read(sqe, timerfd, new_req->buffer, new_req->buffer_size, 0);
                io_uring_sqe_set_data(sqe, new_req);
                io_uring_submit(&ring);
            }
        }
    }

    void handle_client_read(Request* req, int res) {
        int client_fd = req->fd;
        auto it = clients.find(client_fd);

        if (it == clients.end()) {
            delete[] req->buffer;
            delete req;
            return;
        }

        if (res > 0) {
            it->second.addBytes(res);
            if (write_to_stdout(req->buffer, res) != 0) {
                std::cerr << "write to stdout failed" << std::endl;
            }

            delete[] req->buffer;
            delete req;
            submit_client_read(client_fd);
            io_uring_submit(&ring);

        } else if (res == 0) {
            // Client disconnected
            std::cout << "Client " << client_fd << " disconnected" << std::endl;
            delete[] req->buffer;
            delete req;
            remove_client(client_fd);
        } else {
            // Error
            std::cerr << "Client " << client_fd << " read error: " << strerror(-res) << std::endl;
            delete[] req->buffer;
            delete req;
            remove_client(client_fd);
        }
    }

    void remove_client(int fd) {
        auto it = clients.find(fd);
        if (it != clients.end()) {
            close(fd);
            clients.erase(it);
        }
    }

public:
    UringBidirectionalRelay(int sock, bool listen) : sockfd(sock), is_listen(listen) {
        start_time = time(nullptr);
        setup_uring();
    }

    ~UringBidirectionalRelay() {
        for (auto& [fd, _] : clients) {
            close(fd);
        }
        if (timerfd >= 0) close(timerfd);
        io_uring_queue_exit(&ring);
    }

    void exec() {
        while (!stdin_closed && !socket_closed) {
            struct io_uring_cqe *cqe;
            int ret = io_uring_wait_cqe(&ring, &cqe);
            if (ret < 0) {
                if (errno == EINTR) continue;
                throw std::runtime_error("io_uring_wait_cqe failed");
            }

            auto *req = static_cast<Request*>(io_uring_cqe_get_data(cqe));
            int res = cqe->res;

            if (req) {
                switch (req->type) {
                case OpType::STDIN_READ:
                    handle_stdin_read(req, res);
                    break;
                case OpType::SOCKET_READ:
                    handle_socket_read(req, res);
                    break;
                case OpType::ACCEPT:
                    handle_accept(req, res);
                    break;
                case OpType::TIMER_READ:
                    handle_timer_read(req, res);
                    break;
                case OpType::CLIENT_READ:
                    handle_client_read(req, res);
                    break;
                }
            }

            io_uring_cqe_seen(&ring, cqe);
        }
    }
};
void listen_mode_iouring(int port) {
    int listen_fd = create_socket(true, "0.0.0.0", port);
    if (listen_fd < 0) return;

    std::cout << "Listening on port " << port << "...\n";

    UringBidirectionalRelay relay(listen_fd, true);
    relay.exec();
}

void client_mode_iouring(std::string host, int port) {
    int sockfd = create_socket(false, host, port);
    if (sockfd < 0) return;

    UringBidirectionalRelay relay(sockfd, false);
    relay.exec();
}

#endif // IO_URING_H
