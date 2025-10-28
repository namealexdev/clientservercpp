#include "epollserver.h"
#include "utils.h"
#include <sys/timerfd.h>


std::atomic<bool> g_should_stop{false};
void stop_signal_handler(int signal) {
    std::cout << "get stop signal " << signal << " start stopping..." << std::endl;
    g_should_stop = true; //
}

Epoll::Epoll(int sock, bool listen, int show_timer_stats) :
    sockfd(sock), is_listen(listen), show_timer_stats(show_timer_stats)
{
    start_time = time(nullptr);
    setup_epoll();
}


Epoll::~Epoll()
{
    fullstop();
    // if (epfd >= 0) close(epfd);
    // if (timerfd >= 0) close(timerfd);
    // if (event_external_fd >= 0) close(event_external_fd);
}

void Epoll::fullstop()
{
    // Устанавливаем флаг, чтобы цикл exec() завершился
    g_should_stop.store(true);

    // 1. Закрываем основной сокет (если есть), это приведет к событию EPOLLHUP/EPOLLERR
    // и вызовет socket_closed = true в exec(), или обработчику сокета.
    if (sockfd >= 0) {
        close(sockfd);
        sockfd = -1; // Сбросим дескриптор
    }
    // 2. Принудительно удаляем все клиентские сокеты
    //    (включая удаление из epoll, закрытие сокета, удаление из clients)
    //    Используем копию ключей, чтобы избежать итерации по изменяющемуся контейнеру.
    std::vector<int> client_fds_to_close;
    {
        // Блокировка не нужна, если fullstop вызывается из потока, где
        // больше никто не взаимодействует с clients.
        // Если возможно параллельное изменение clients из другого места,
        // нужен мьютекс.
        for (const auto& [fd, stats] : clients) {
            client_fds_to_close.push_back(fd);
        }
    }
    for (int fd : client_fds_to_close) {
        remove_client(fd); // remove_client сам удаляет из epoll, закрывает fd и удаляет из clients
    }
    clients.clear();
    // 4. Очищаем очередь внешних сокетов
    {
        std::lock_guard<std::mutex> lock(mtx_pending_new_socks_);
        while (!pending_new_socks_.empty()) {
            auto& [fd, stats] = pending_new_socks_.front();
            // Сокет, находящийся в очереди, должен быть закрыт и его ресурсы освобождены
            close(fd); // Закрываем сокет, который не был добавлен в epoll
            // Stats объект (stats) будет уничтожен при удалении из очереди
            pending_new_socks_.pop();
        }
    }

    if (epfd >= 0) {
        close(epfd);
        epfd = -1;
    }
    if (timerfd >= 0) {
        close(timerfd);
        timerfd = -1;
    }
    if (event_external_fd >= 0) {
        close(event_external_fd);
        event_external_fd = -1;
    }
    stdin_closed = true;
    socket_closed = true;
    size_clients = 0;
}

void Epoll::exec()
{
    const int MAX_EVENTS = 4;
    epoll_event events[MAX_EVENTS];
    const int EPOLL_TIMEOUT = 1000;

    while (!stdin_closed && !socket_closed) {
        if (g_should_stop.load()){
            break;
        }
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
                    std::unique_lock lock_clients(mtx_clients);// запись
                    while (!pending_new_socks_.empty()) {
                        auto data = pending_new_socks_.front(); pending_new_socks_.pop();
                        add_fd(data.first, EPOLLIN | EPOLLRDHUP);

                        // size_clients уже добавили
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
    timer_spec.it_value.tv_sec = TIMER_STATS_TIMEOUT_SECS;    // Первый сработает через 5 сек
    timer_spec.it_value.tv_nsec = 0;
    timer_spec.it_interval.tv_sec = TIMER_STATS_TIMEOUT_SECS; // Повторять каждую секунду
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
        std::cout << "fail epoll_ctl add " << fd << std::endl;
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

        {
            std::unique_lock lock(mtx_clients); // чтение - Запись? unique_lock
            clients[fd].addBytes(n);
        }
        if (write_to_stdout(buffer, SERVER_WRITE_STDOUT?n:0) != 0) {
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

#include "string.h"
void Epoll::accept_connections()
{
    while (true) {
        sockaddr_in client_addr{};
        socklen_t addr_len = sizeof(client_addr);
        int client_fd = accept4(sockfd, (sockaddr*)&client_addr, &addr_len, SOCK_NONBLOCK | SOCK_CLOEXEC);
        if (client_fd == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                break;
            throw std::runtime_error(std::string("accept4: ") + strerror(errno));
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
            // если надо добавить все в текущем потоке, то очередь для сокетов не нужна
            // std::cout << "not balance_socket " << std::endl;
            if (!add_fd(client_fd, EPOLLIN | EPOLLRDHUP )){
                close(client_fd);
            }else{
                size_clients++;
                clients.emplace(client_fd, std::move(st));
                // std::cout << "Added socket: " << client_fd << " (" << clients[client_fd].ip << ")" << std::endl;
            }

        }

    }
}

// collect stats (and send stats to socket)
void Epoll::handle_timer()
{
    uint64_t expirations;
    if (read(timerfd, &expirations, sizeof(expirations)) != sizeof(expirations)) return;

    // update stats
    {
        std::unique_lock lock(mtx_clients); // чтение (запись)
        for (auto& [fd, stats] : clients) {
            stats.updateBps();
            std::string stats_msg;
            if (stats.checkFourGigabytes(stats_msg)) {
                send(fd, stats_msg.c_str(), stats_msg.size(), MSG_NOSIGNAL);
            }
        }
    }


    if (show_timer_stats){
        time_t now = time(nullptr);
        std::cout << "======== Uptime: " << (now - start_time) << " s\n";
        show_shared_stats();
    }

}

void Epoll::remove_client(int fd) {
    // count_fd--;
    epoll_ctl(epfd, EPOLL_CTL_DEL, fd, nullptr);
    {
        std::unique_lock lock(mtx_clients);// запись
        clients.erase(fd);
    }

    size_clients--;
    close(fd);
}

uint64_t Epoll::print_clients_stats()
{
    uint64_t total_bps = 0;
    std::shared_lock lock(mtx_clients);// чтение
    for (auto& c: clients){
        std::cout << "\n" << c.second.get_stats();
        total_bps += c.second.current_bps;
    }
    return total_bps;
}

void Epoll::push_external_socket(int client_fd, const Stats &st) {
    {
        std::lock_guard lock(mtx_pending_new_socks_);
        pending_new_socks_.push(std::make_pair(client_fd, std::move(st)));
        // добавляем тут, чтобы балансировка проходила корректно
        size_clients++;
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
#include <thread>
#include <random>
std::vector<uint8_t> generateRandomData(size_t size)
{
    std::vector<uint8_t> data(size);
    static std::mt19937 gen(std::random_device{}());
    static std::uniform_int_distribution<uint8_t> dis(0, 255);

    for (size_t i = 0; i < size; ++i) {
        data[i] = dis(gen);
    }
    return data;
}

void client_mode_epoll(std::string host, int port)
{
    int sockfd = create_socket(false, host, port);

    if (sockfd < 0) return;

    if (CLIENT_SELF_SEND_1gbps)
    new std::thread([sockfd](){
        // auto v = generateRandomData(2048);
        // while (1){
        //     write(sockfd, v.data(), v.size());
        //     // std::this_thread::sleep_for(std::chrono::milliseconds(100));
        // }
        constexpr double TARGET_GBPS = 1.0; // Целевая скорость в ГБит/с
        constexpr double TARGET_MBPS = TARGET_GBPS * 1000.0; // 1000 МБит/с
        constexpr size_t TARGET_BPS = static_cast<size_t>(TARGET_MBPS * 1000.0 * 1000.0 / 8.0); // в байтах/с (бит/с / 8)

        constexpr size_t BUFFER_SIZE = 64 * 1024; // Размер кусочка данных, отправляемого за раз (в байтах)
        constexpr auto INTERVAL = std::chrono::milliseconds(100); // Интервал времени для усреднения отправки (в миллисекундах)

        auto v = generateRandomData(BUFFER_SIZE);
        auto interval_start = std::chrono::steady_clock::now();
        size_t bytes_sent_in_interval = 0;

        while (true) { // Бесконечный цикл
            auto now = std::chrono::steady_clock::now();
            auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - interval_start).count();

            if (elapsed_ms >= INTERVAL.count()) {
                interval_start = now;
                bytes_sent_in_interval = 0;
            } else {
                // Вычисляем, сколько байт можно отправить за прошедшее время в текущем интервале
                size_t bytes_allowed_in_interval = (TARGET_BPS * elapsed_ms) / 1000;
                if (bytes_sent_in_interval >= bytes_allowed_in_interval) {
                    // Если отправлено больше, чем разрешено за это время, ждем
                    std::this_thread::sleep_for(std::chrono::microseconds(1000));
                    continue;
                }
            }

            // Отправляем фиксированный размер буфера (или его часть, если буфер больше TARGET_BPS * INTERVAL)
            size_t bytes_to_send = std::min(BUFFER_SIZE, static_cast<size_t>((TARGET_BPS * INTERVAL.count()) / 1000));

            ssize_t sent = write(sockfd, v.data(), bytes_to_send);

            if (sent > 0) {
                bytes_sent_in_interval += sent;
            } else if (sent == -1) {
                if (errno != EAGAIN && errno != EWOULDBLOCK) {
                    // Произошла ошибка записи (например, сокет закрыт, сеть недоступна)
                    // Выходим из цикла
                    break;
                }
                // Если errno == EAGAIN или EWOULDBLOCK, буфер сокета полон, просто ждем следующую итерацию
            }
        }
    });


    Epoll e(sockfd, false, false);
    e.exec();
}
