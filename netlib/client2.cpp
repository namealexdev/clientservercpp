#include "client2.h"



SimpleClientEventfd::SimpleClientEventfd(ClientConfig cfg)
    : IClient(std::move(cfg))
{
    // create epoll
    epfd_ = epoll_create1(EPOLL_CLOEXEC);
    if (epfd_ < 0) throw std::runtime_error("epoll_create1 failed");

    // event для QueueAdd
    event_fd_ = eventfd(0, EFD_NONBLOCK | EFD_SEMAPHORE);
    if (event_fd_ < 0) {
        close(epfd_);
        throw std::runtime_error("eventfd failed");
    }
    epoll_event ev{};
    ev.events  = EPOLLIN;
    ev.data.fd = event_fd_;
    if (epoll_ctl(epfd_, EPOLL_CTL_ADD, event_fd_, &ev) < 0)
        throw std::runtime_error("epoll_ctl ADD eventfd failed");
}

SimpleClientEventfd::~SimpleClientEventfd()
{
    Stop();
    if (event_fd_ >= 0) close(event_fd_);
    if (epfd_ >= 0) close(epfd_);
}

int SimpleClientEventfd::SendToSocket(char* data, uint32_t size)
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

    stats_.AddSendBytes(total_sent);
    return total_sent;
}

void SimpleClientEventfd::AddHandlerEvent(EventType type, std::function<void (void *)> handler){
    if (!dispatcher_){
        dispatcher_ = new EventDispatcher;
    }
    if (handler){
        dispatcher_->unsetHandler(type);
    }else{
        dispatcher_->setHandler(type, std::move(handler));
    }
}

void SimpleClientEventfd::SwitchAsyncQueue(bool enable) {
    conf_.auto_send = enable;
}

void SimpleClientEventfd::Start()
{
    running_ = true;
    epoll_thread_ = std::make_unique<std::thread>(&SimpleClientEventfd::epollLoop, this);
}

void SimpleClientEventfd::Stop()
{
    if (!running_) return;

    running_ = false;

    uint64_t v = 1;
    if (event_fd_ >= 0) write(event_fd_, &v, sizeof(v));

    if (epoll_thread_ && epoll_thread_->joinable()){
        epoll_thread_->join();
    }
    epoll_thread_.reset();

    closeSocket();
}

bool SimpleClientEventfd::QueueAdd(char* data, int size)
{
    QueueItem qi{data, (size_t)size, 0};

    bool was_empty = queue_.empty();
    if (!queue_.push(qi)) {
        // d("QueueAdd: queue full");
        return false;
    }

    if (conf_.auto_send && was_empty) {
        uint64_t v = 1;
        write(event_fd_, &v, sizeof(v));
    }
    return true;
}


// int SimpleClientEventfd::makeNonBlocking(int fd)
// {
//     int flags = fcntl(fd, F_GETFL, 0);
//     if (flags < 0) return -1;
//     return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
// }

void SimpleClientEventfd::closeSocket()
{
    if (socket_ >= 0) {
        epoll_ctl(epfd_, EPOLL_CTL_DEL, socket_, nullptr);
        close(socket_);
        socket_ = -1;
    }
}

bool SimpleClientEventfd::enableEpollOut()
{
    if (socket_ < 0) return false;
    epoll_event ev{};
    ev.events  = EPOLLIN | EPOLLOUT | EPOLLRDHUP | EPOLLERR;
    ev.data.fd = socket_;
    return epoll_ctl(epfd_, EPOLL_CTL_MOD, socket_, &ev) == 0;
}

bool SimpleClientEventfd::disableEpollOut()
{
    if (socket_ < 0) return false;
    epoll_event ev{};
    ev.events  = EPOLLIN | EPOLLRDHUP | EPOLLERR;
    ev.data.fd = socket_;
    return epoll_ctl(epfd_, EPOLL_CTL_MOD, socket_, &ev) == 0;
}


bool SimpleClientEventfd::reconnect()
{
    if (state_ == ClientState::CONNECTING){
        return false;
    }
    state_ = ClientState::CONNECTING;
    int attempt = 0;

    while (running_) {
        int sock = create_socket_connect();
        if (sock >= 0) {
            socket_ = sock;
            // makeNonBlocking(socket_fd_);

            epoll_event ev{};
            ev.events  = EPOLLIN | EPOLLRDHUP | EPOLLERR;
            ev.data.fd = socket_;
            epoll_ctl(epfd_, EPOLL_CTL_ADD, socket_, &ev);

            state_ = ClientState::WAITING;
            return true;
        }

        attempt++;
        if (!conf_.auto_reconnect) {
            state_ = ClientState::ERROR;
            return false;
        }

        int backoff_ms = std::min(1000, 50 * (1 << std::min(attempt, 6)));
        std::this_thread::sleep_for(std::chrono::milliseconds(backoff_ms));
    }
    return false;
}

void SimpleClientEventfd::handleData()
{
    while (true) {
        ssize_t n = recv(socket_, buffer_, sizeof(buffer_), MSG_DONTWAIT);
        if (n > 0) {
            stats_.AddRecvBytes(n);
            continue;
        }
        if (n == 0) {
            onSocketClosed();//reconnect?
            return;
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return;

        d("recv error: " << strerror(errno));
        last_error_ = strerror(errno);
        onSocketClosed();// reconnect?
        return;
    }
}

void SimpleClientEventfd::onSocketClosed()
{
    closeSocket();
    if (conf_.auto_reconnect){
        reconnect();
    }else {
        state_ = ClientState::ERROR;
        running_ = false;
    }
}

bool SimpleClientEventfd::QueueSendAll()
{
    // так как у нас lockfree, то батчинга нету и делаем это последовательно 1 пакет = 1 sendmsg
    while (!queue_.empty()) {
        QueueItem &cur = queue_.front();
        size_t remaining = cur.size - cur.sent_bytes;

        if (remaining > 0) {
            int sent = SendToSocket(cur.data + cur.sent_bytes,
                                    remaining);

            if (sent == -1) {
                state_ = ClientState::ERROR;
                last_error_ = strerror(errno);
                onSocketClosed();// reconnect?
                return false; // ошибка
            } else if (sent > 0) {
                cur.sent_bytes += sent;
            } else {
                // break; // не удалось отправить сейчас
                enableEpollOut();
                return false; // выходим, но очередь не пуста
            }
        }


        // Если сообщение полностью отправлено
        if (cur.sent_bytes >= cur.size) {
            queue_.pop(cur);
        }
    }

    disableEpollOut();
    return true;
}

void SimpleClientEventfd::epollLoop()
{
    if (!reconnect()){
        return;
    }

    epoll_event events[EPOLL_MAX_EVENTS];
    while (running_) {
        int n = epoll_wait(epfd_, events, EPOLL_MAX_EVENTS, -1);
        if (n < 0) {
            if (errno == EINTR) continue;
            d("epoll_wait error: " << strerror(errno));
            break;
        }

        for (int i = 0; i < n; ++i) {
            int fd = events[i].data.fd;
            uint32_t ev = events[i].events;

            // отправка по event, когда делаем push и когда EPOLLOUT + auto_send
            // но если часто делать push, то постоянно будут поступать сюда события ...
            if (fd == event_fd_) {
                uint64_t v;
                read(event_fd_, &v, sizeof(v));
                QueueSendAll();
                continue;
            }

            if (fd == socket_) {
                if (ev & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) {
                    onSocketClosed();// reconnect - ok
                    continue;
                }
                if (ev & EPOLLIN){
                    handleData();
                }
                if (ev & EPOLLOUT && conf_.auto_send){
                    QueueSendAll();
                }
            }
        }
    }
    closeSocket();
}
