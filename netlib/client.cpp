#include "client.h"
#include <cassert>
// #include "serialization.h"

int IClient::create_socket_connect()
{
    int sock;
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        last_error_ = "socket failed";
        return -1;
    }

    // setsockopt(sock, SOL_SOCKET, SO_SNDBUF, &conf_.send_buffer_size, sizeof(conf_.send_buffer_size));

    if (!conf_.host.empty()){
        struct sockaddr_in bind_addr;
        bind_addr.sin_family = AF_INET;
        if (inet_pton(AF_INET, conf_.host.c_str(), &bind_addr.sin_addr) <= 0) {
            last_error_ = "inet_pton bind failed " + conf_.host;
            close(sock);
            return -1;
        }
        if (bind(sock, reinterpret_cast<struct sockaddr*>(&bind_addr), sizeof(bind_addr)) < 0) {
            last_error_ = "bind failed";
            close(sock);
            return -1;
        }
    }

    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(conf_.server_port);
    if (inet_pton(AF_INET, conf_.server_ip.c_str(), &serv_addr.sin_addr) <= 0) {
        last_error_ = "inet_pton failed " + conf_.host;
        close(sock);
        return -1;
    }

    if (::connect(sock, reinterpret_cast<struct sockaddr*>(&serv_addr), sizeof(serv_addr)) < 0) {
        last_error_ = "connection failed";
        close(sock);
        return -1;
    }

    return sock;
}

string IClient::GetClientState()
{
    switch (state_) {
        case ClientState::DISCONNECTED: return "DISCONNECTED";
        case ClientState::CONNECTING: return "CONNECTING";
        case ClientState::HANDSHAKE: return "HANDSHAKE";
        case ClientState::WAITING: return "WAITING";
        case ClientState::SENDING: return "SENDING";
        case ClientState::ERROR: return "ERROR: " + last_error_;
        default: return "UNKNOWN";
    }
}

SimpleClient::SimpleClient(ClientConfig config):
    IClient(std::move(config)) {

    epoll_.SetOnReadAcceptHandler([&](int fd) {
        // accept тут нету
        // assert(socket_ == fd);
        handleData();
    });

    epoll_.SetOnReconnectHandler([&](){
        d("reconnect " << conf_.auto_send)
        reconnect();
        if (conf_.auto_send){
            SwitchAsyncQueue(conf_.auto_send);
        }
        d("stop reconnect")
    });

    epoll_.SetOnDisconnectHandler([&](int fd){
        // d("disconnect handle " << fd)
        if (fd == socket_ && conf_.auto_reconnect){
            epoll_.RemoveFd(fd);
            reconnect();
        }else{
            Stop();
        }
        if (dispatcher_) {
            dispatcher_->onEvent(EventType::ClientDisconnected, &fd);
        }
    });

    epoll_.SetOnReadyWriteHandler([&](int fd){
        d("onwrite " << fd);

        if (async_queue_send_){
            state_ = ClientState::SENDING;
            futex_wake_queue();
            // сюда поподаем если при QueueSendAll произошел EnableWriteEvents
            epoll_.DisableWriteEvents(fd);
        }else{
            // Отправка в epoll-потоке
            if (QueueSendAll()){
                // очередь пустая -> больше не ждём EPOLLOUT
                state_ = ClientState::WAITING;
                epoll_.DisableWriteEvents(fd);
            }else{
                state_ = ClientState::SENDING;
                // EnableWriteEvents уже включен в QueueSendAll
            }
        }

        if (dispatcher_){
            dispatcher_->onEvent(EventType::WriteReady);
        }
    });
    if (!conf_.auto_reconnect && conf_.auto_send){
        SwitchAsyncQueue(conf_.auto_send);
    }
}

SimpleClient::~SimpleClient()
{
    // d("~SimpleClient start")
    SwitchAsyncQueue(0);
    epoll_.StopEpoll();
    // d("~SimpleClient end")
}

void SimpleClient::Start()
{
    state_ = ClientState::CONNECTING;
    epoll_.RunEpoll(true);
}

// блокирует пока не приконектиться. вызывается из потока BaseEpoll.
void SimpleClient::reconnect()
{
    // d("client reconnected")
    state_ = ClientState::CONNECTING;
    while(state_ != ClientState::DISCONNECTED){
        auto sock = create_socket_connect();
        if (sock < 0){
            if (!conf_.auto_reconnect){
                break;
            }
            continue;
        }
        socket_ = sock;
        epoll_.AddFd(sock);
        state_ = ClientState::WAITING;
        break;
    }
}

// void SimpleClient::StartWaitConnect()
// {
//     auto sock = create_socket_connect();
//     if (sock < 0){
//         state_ = ClientState::ERROR;
//         return ;
//     }

//     socket_ = sock;
//     epoll_.AddFd(sock);
//     epoll_.RunEpoll();

//     state_ = ClientState::WAITING;
// }

void SimpleClient::Stop()
{
    state_ = ClientState::DISCONNECTED;
    epoll_.RemoveFd(socket_);
    socket_ = -1;
    epoll_.StopEpoll();
    queue_.reset();
}

// int SimpleClient::SendToSocket(char *data, uint32_t size)
// {
//     if (socket_ < 0) return -1;
//     if (size == 0) return 0;

//     uint32_t net_size = htonl(static_cast<uint32_t>(size));

//     iovec iov[2];
//     iov[0].iov_base = &net_size;
//     iov[0].iov_len = sizeof(net_size);
//     iov[1].iov_base = data;
//     iov[1].iov_len = size;

//     msghdr msg = {};
//     msg.msg_iov = iov;
//     msg.msg_iovlen = 2;

//     // d("SendToSocket")
//     ssize_t sent = sendmsg(socket_, &msg, MSG_NOSIGNAL | MSG_DONTWAIT);
//     // d("sendmsg " << size+ sizeof(size));

//     if (sent < 0) {
//         if (errno == EAGAIN || errno == EWOULDBLOCK) {
//             // std::cerr << "sock blocked" << std::endl;
//             return 0; // сокет временно недоступен для записи
//         }
//         // std::cerr << sent << " send failed: " << strerror(errno) << std::endl;
//         return -1;
//     }

//     stats_.addBytes(sent);
//     return sent;
// }

int SimpleClient::SendToSocket(char* data, uint32_t size)
{
    if (socket_ < 0) return -1;
    if (size == 0)   return 0;

    uint32_t net_size = htonl(size);

    // 1) первая попытка — одним вызовом sendmsg
    size_t total_sent;
    size_t total_needed = sizeof(net_size) + size;

    int r = sendHeaderPayloadOnce(socket_, net_size, data, size, total_sent);
    if (r < 0) return -1;        // ошибка
    if (total_sent == 0) return 0; // EAGAIN EWOULDBLOCK

    // всё отправлено?
    if (total_sent != total_needed) {
        // досылаем остаток
        if (sendRemainder(socket_, net_size, data, size, total_sent) < 0)
            return -1;
    }

    stats_.addBytes(total_sent);
    return total_sent;
}

bool SimpleClient::QueueAdd(char *data, int size){
    QueueItem item{data, size, 0};
    bool was_empty = is_queue_empty();
    bool ok = push_item(std::move(item));//queue_.push(std::move(item));
    if (!ok) return false;

    if (was_empty) {
        if (async_queue_send_) {
            // говорим очереди что готовы читать
            futex_wake_queue();
        } else {
            // узнаем когда может писать
            // d("add")
            epoll_.EnableWriteEvents(socket_);
        }
    }
    return true;
}

bool SimpleClient::QueueSendAll(){
    // так как у нас lockfree, то батчинга нету и делаем это последовательно 1 пакет = 1 sendmsg
    while (!is_queue_empty()) {
        auto& cur = front_item();
        size_t remaining = cur.size - cur.sent_bytes;

        if (remaining > 0) {
            int sent = SendToSocket(cur.data + cur.sent_bytes,
                                    remaining);

            if (sent == -1) {
                state_ = ClientState::ERROR;
                return false; // ошибка
            } else if (sent > 0) {
                cur.sent_bytes += sent;
            } else {
                // break; // не удалось отправить сейчас
                // для отдельного потока

                // d("fail send")
                epoll_.EnableWriteEvents(socket_);
                return false; // выходим, но очередь не пуста
            }
        }


        // Если сообщение полностью отправлено
        if (cur.sent_bytes >= cur.size) {
            pop_item(cur);
        }
    }

    // epoll_.DisableWriteEvents(socket_);
    return true;
}

bool SimpleClient::push_item(const QueueItem &&item) {
    return queue_.push(item);
}

QueueItem &SimpleClient::front_item() {
    return queue_.front();
}

bool SimpleClient::pop_item(QueueItem &item) {
    return queue_.pop(item);
}

bool SimpleClient::is_queue_empty() {
    return queue_.empty();
}

void SimpleClient::futex_wake_queue() noexcept {
    futex_flag_.store(1, std::memory_order_release);

    syscall(SYS_futex,
            reinterpret_cast<int*>(&futex_flag_),
            FUTEX_WAKE, 1,
            nullptr, nullptr, 0);
}

void SimpleClient::futex_wait_queue() noexcept {
    futex_flag_.store(0, std::memory_order_relaxed);

    syscall(SYS_futex,
            reinterpret_cast<int*>(&futex_flag_),
            FUTEX_WAIT, 0,
            nullptr, nullptr, 0);
}

// когда прилетает событие делаем notify
void SimpleClient::SwitchAsyncQueue(bool enable)
{
    d("async queue " << enable << " before: " << async_queue_send_)
    if (async_queue_send_ && enable){
        return;
    }

    async_queue_send_ = enable;
    if (!async_queue_send_){
        if (queue_th_){
            // останавливаем очередь
            futex_wake_queue();
            if (queue_th_->joinable()){
                queue_th_->join();
            }
            delete queue_th_;
            queue_th_ = nullptr;
        }
        return;
    }

    queue_th_ = new std::thread([this](){
        d("queue_th_ start " << async_queue_send_ << " " << (int)state_ << " " << (async_queue_send_ && state_ != ClientState::DISCONNECTED))
        while (async_queue_send_ && state_ != ClientState::DISCONNECTED) {
            // d("loop start");

            if (socket_ <= 0 || !IsConnected()) {
                d("ftx WAIT: no socket ");
                futex_wait_queue();
                // epoll_.EnableWriteEvents(socket_);
                continue;
            }

            if (is_queue_empty()) {
                d("ftx WAIT: empty queue");
                futex_wait_queue();
                // epoll_.EnableWriteEvents(socket_);
                continue;
            }

            d("SENDING...");
            if (!QueueSendAll()){
                futex_wait_queue();
            }
        }
        d("queue_th_ end")
    });
}

void SimpleClient::AddHandlerEvent(EventType type, std::function<void (void *)> handler){
    if (!dispatcher_){
        dispatcher_ = new EventDispatcher;
    }
    if (handler){
        dispatcher_->unsetHandler(type);
    }else{
        dispatcher_->setHandler(type, std::move(handler));
    }
}

// ответы от сервера (пока нету)
// void SimpleClient::handleData(){
//     ssize_t n;
//     // это не SubEpoll, тут не нужна статистика
//     // std::cout << "2handle_socket_data " << n << std::endl;
//     n = recv(socket_, buffer_, sizeof(buffer_), 0);
//     if (n > 0) {
//         if (dispatcher_) {
//             dispatcher_->onEvent(EventType::DataReceived, &n);
//         }
//         // clientHandler_->onEvent()
//         // if (on_recv_handler)
//         //     on_recv_handler(buffer, n);
//         // if (write_to_stdout(buffer, n) != 0) {
//         //     throw std::runtime_error("write to stdout");
//         // }
//     } else if (n == 0) {
//         close(socket_);
//         socket_ = -1;
//     } else {
//         throw std::runtime_error("recv from socket");
//     }
// }

void SimpleClient::handleData() {
    // epoll ET - read all
    while (true) {
        ssize_t n = recv(socket_, buffer_, sizeof(buffer_), MSG_DONTWAIT);
        if (n > 0) {
            if (dispatcher_) {
                DataReceived d;
                d.data = buffer_;
                d.size = n;
                dispatcher_->onEvent(EventType::DataReceived, &d);
            }
            continue;
        }
        if (n == 0) {
            // Удаленный хост закрыл соединение
            Stop();
            break;
        }

        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            break;
        }

        last_error_ = strerror(errno);
        state_ = ClientState::ERROR;
        Stop();
        return;
    }
}
