#include "client.h"
#include <cassert>
// #include "serialization.h"

int IClient::create_socket_connect()
{
    int sock;
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
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
        handleData();
    });

    epoll_.SetOnDisconnectHandler([&](int fd){
        // d("disconnect handle " << fd)
        if (fd == -2){// connect in loop before start
            reconnect();
            return ;
        }
        if (fd == socket_ && conf_.auto_reconnect){
            epoll_.RemoveFd(fd);
            reconnect();
            return;
        }else{
            Stop();
            return;
        }
        if (dispatcher_) {
            dispatcher_->onEvent(EventType::ClientDisconnected, &fd);
        }
    });

    epoll_.SetOnReadyWriteHandler([&](int fd){
        if (dispatcher_){
            dispatcher_->onEvent(EventType::WriteReady);
        }
        if (async_queue_send_){
            send_queue_cv_.notify_one();
        }else{
            state_ = ClientState::SENDING;
            if (QueueSendAll()){
                state_ = ClientState::WAITING;
            }
        }
    });

    if (conf_.auto_send){
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

// bool RecvMsg(int fd, void* buf, size_t size) {
//     char* ptr = static_cast<char*>(buf);
//     size_t total = 0;
//     while (total < size) {
//         ssize_t n = recv(fd, ptr + total, size - total, MSG_WAITALL);
//         if (n <= 0) return false;
//         total += n;
//     }
//     return true;
// }


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
            continue;
        }
        epoll_.AddFd(sock);
        socket_ = sock;
        state_ = ClientState::WAITING;
        break;
    }
}

void SimpleClient::StartWaitConnect()
{
    auto sock = create_socket_connect();
    if (sock < 0){
        state_ = ClientState::ERROR;
        return ;
    }

    socket_ = sock;
    epoll_.AddFd(sock);
    epoll_.RunEpoll();

    state_ = ClientState::WAITING;
}

void SimpleClient::Stop()
{
    state_ = ClientState::DISCONNECTED;
    epoll_.RemoveFd(socket_);
    socket_ = -1;
    epoll_.StopEpoll();
}

int SimpleClient::SendToSocket(char *data, uint32_t size){

    uint32_t net_size = htonl(static_cast<uint32_t>(size));

    iovec iov[2];
    iov[0].iov_base = &net_size;
    iov[0].iov_len = sizeof(net_size);
    iov[1].iov_base = data;
    iov[1].iov_len = size;

    msghdr msg = {};
    msg.msg_iov = iov;
    msg.msg_iovlen = 2;

    // d("sendmsg " << size+ sizeof(size));
    ssize_t sent = sendmsg(socket_, &msg, MSG_NOSIGNAL | MSG_DONTWAIT);

    if (sent < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // std::cerr << "sock blocked" << std::endl;
            return 0; // сокет временно недоступен для записи
        }
        std::cerr << sent << " send failed: " << strerror(errno) << std::endl;
        return -1;
    }

    stats_.addBytes(sent);
    return sent;
}

void SimpleClient::QueueAdd(char *data, int size){
    std::lock_guard<std::mutex> lock(queue_mtx_);
    queue_.push(QueueItem{data, size, 0});
    if (async_queue_send_) {
        send_queue_cv_.notify_one();
    }
}

bool SimpleClient::QueueSendAll(){
    // надо ждать события чтобы продолжить, чтобы не грузился cpu ???
    // если отправилась только часть, то надо сохранить может значение
    // с какого читать надо и вставить обратно в очередь, но чтобы порядок сохранился
    std::lock_guard<std::mutex> lock(queue_mtx_);
    return queueSendAllUnsafe();
}

bool SimpleClient::queueSendAllUnsafe()
{
    while (!queue_.empty()) {
        auto& current_item = queue_.front();
        size_t remaining = current_item.size - current_item.sent_bytes;

        if (remaining > 0) {
            int sent = SendToSocket(current_item.data + current_item.sent_bytes,
                                    remaining);

            if (sent == -1) {
                state_ = ClientState::ERROR;
                return false; // ошибка
            } else if (sent > 0) {
                current_item.sent_bytes += sent;
            } else {
                // break; // не удалось отправить сейчас
                return false; // выходим, но очередь не пуста
            }
        }


        // Если сообщение полностью отправлено
        if (current_item.sent_bytes >= current_item.size) {
            queue_.pop();
        }
    }

    return queue_.empty();
}

// когда прилетает событие делаем notify
void SimpleClient::SwitchAsyncQueue(bool enable)
{
    d("async queue " << enable)
    async_queue_send_ = enable;
    if (!async_queue_send_){
        if (queue_th_){
            // stop
            send_queue_cv_.notify_all();
            if (queue_th_->joinable()){
                queue_th_->join();
            }
            delete queue_th_;
            queue_th_ = nullptr;
        }
        return;
    }

    queue_th_ = new std::thread([this](){
        std::unique_lock lock(queue_mtx_);
        while(async_queue_send_ && state_ != ClientState::DISCONNECTED) {
            // Если очередь пуста - ждем данных
            if (queue_.empty()) {
                state_ = ClientState::WAITING;

                send_queue_cv_.wait(lock, [&]() {

                    return !async_queue_send_ ||
                           state_ == ClientState::DISCONNECTED ||
                           !queue_.empty();
                });
                continue;
            }
            state_ = ClientState::SENDING;

            queueSendAllUnsafe();
        }
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
void SimpleClient::handleData(){
    ssize_t n;
    // это не SubEpoll, тут не нужна статистика
    // std::cout << "2handle_socket_data " << n << std::endl;
    n = recv(socket_, buffer_, sizeof(buffer_), 0);
    if (n > 0) {
        if (dispatcher_) {
            dispatcher_->onEvent(EventType::DataReceived, &n);
        }
        // clientHandler_->onEvent()
        // if (on_recv_handler)
        //     on_recv_handler(buffer, n);
        // if (write_to_stdout(buffer, n) != 0) {
        //     throw std::runtime_error("write to stdout");
        // }
    } else if (n == 0) {
        close(socket_);
        socket_ = -1;
    } else {
        throw std::runtime_error("recv from socket");
    }
}

