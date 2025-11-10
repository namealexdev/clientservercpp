#include "epoll.h"

bool BaseEpoll::AddFd(int fd)
{
    const uint32_t events = EPOLLIN | EPOLLRDHUP;
    epoll_event ev{.events = events, .data{.fd = fd}};
    if (epoll_ctl(epfd_, EPOLL_CTL_ADD, fd, &ev) == -1) {
        // throw std::runtime_error("epoll_ctl add");
        std::cout << "fail epoll_ctl add " << fd << " error: " << strerror(errno)<< std::endl;
        return false;
    }
    return true;
}

void BaseEpoll::RemoveFd(int fd)
{
    epoll_ctl(epfd_, EPOLL_CTL_DEL, fd, nullptr);
    close(fd);
}

void BaseEpoll::RunEpoll(){
    if (thLoop_){
        StopEpoll();
        thLoop_->join();
        thLoop_.reset();
    }
    thLoop_ = std::make_unique<std::thread>([this](){ExecLoop();});
}

void BaseEpoll::StopEpoll(){
    need_stop_ = true;
}

BaseEpoll::BaseEpoll(){
    epfd_ = epoll_create1(EPOLL_CLOEXEC);
    if (epfd_ == -1) throw std::runtime_error("epoll_create1");
}

BaseEpoll::~BaseEpoll(){
    if (epfd_ >= 0) {
        close(epfd_);
    }
}

void BaseEpoll::ExecLoop()
{
    static epoll_event events[MAX_EVENTS];

    while (!need_stop_) {
        int nfds = epoll_wait(epfd_, events, MAX_EVENTS, EPOLL_TIMEOUT);
        if (nfds == -1) {
            if (errno == EINTR) continue;
            throw std::runtime_error("epoll_wait");
        }

        for (int i = 0; i < nfds; ++i) {

            auto fd = events[i].data.fd;
            auto ev = events[i].events;

            switch (ev & (EPOLLIN | EPOLLOUT | EPOLLERR | EPOLLHUP | EPOLLRDHUP)) {

            // Разрыв соединения
            case EPOLLHUP:
            case EPOLLRDHUP:
            case EPOLLHUP | EPOLLRDHUP:
                if (ev & EPOLLERR){
                    int error = 0;
                    socklen_t len = sizeof(error);
                    if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len) == 0 && error != 0) {
                        d("Socket error on fd " << fd << ": " << strerror(error));
                    }
                }
                if (on_hangup_) on_hangup_(fd);
                break;

            // Доступны данные для чтения и записи
            case EPOLLIN:// accept
                if (on_read_) on_read_(fd);
                break;

            case EPOLLOUT:
                if (on_write_) on_write_(fd);
                break;
            case EPOLLIN | EPOLLOUT:
                if (on_read_) on_read_(fd);
                if (on_write_) on_write_(fd);
                break;

            default:

                std::cout << "Unknown event combination: 0x" << std::hex << ev << " on fd " << fd << "\n";
                throw std::runtime_error("Unknown epoll event ");
                if (on_hangup_) on_hangup_(fd);
                break;
            }

            // if (onEvent){
            //     onEvent(events[i].data.fd, events[i].events);
            // }
        }
    }
}


