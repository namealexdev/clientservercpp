#ifndef CLIENT_H
#define CLIENT_H

#include "const.h"
#include "epoll.h"
#include "libinclude/iclient.h"

#include <condition_variable>
#include <mutex>

// #include "facade_interface_queue.h"

#pragma push_macro("d")
#undef d
#include <boost/lockfree/spsc_queue.hpp>
#pragma pop_macro("d")

#include <linux/futex.h>
#include <sys/syscall.h>



class SimpleClient : public IClient{
public:
    explicit SimpleClient(ClientConfig config);
    ~SimpleClient();

    // чтобы сразу начинать и автоматом делать реконект (асинхронный)
    void Start();
    // sync connect и start
    void StartWaitConnect();
    void Stop();
    bool IsConnected();

    int SendToSocket(char* data, uint32_t size);
    void QueueAdd(char* data, int size);
    // sync send
    bool QueueSendAll();
    void SwitchAsyncQueue(bool enable);

    void AddHandlerEvent(EventType type, std::function<void(void*)> handler);

private:
    void handleData();
    void reconnect();

    int socket_ = -1;
    EventDispatcher* dispatcher_ = 0;
    BaseEpoll epoll_;

    char buffer_[BUF_READ_SIZE];// for recv data from server

    // WARN: поток и мутекс нужны только для мультипотока
    // sync push pop для потока
    // TODO: change to lockfree (чтобы убрать mutex)
    std::thread* queue_th_ = 0;
    // std::condition_variable send_queue_cv_;
    bool async_queue_send_ = false;
    bool queueSendAllUnsafe();

    struct QueueItem {
        char* data;
        int size = 0;
        int sent_bytes = 0;  // сколько байт уже отправлено
    };
    // std::mutex queue_mtx_;
    // std::queue<QueueItem> queue_;
    boost::lockfree::spsc_queue<QueueItem> queue_{100};

    // Методы для работы с очередью
    bool push_item(const QueueItem&& item) {
        bool ok = queue_.push(item);
        if (async_queue_send_ && ok) {
            futex_wake_queue();
        }
        return ok;
    }

    // может поменяться sent_bytes
    QueueItem& front_item() {
        return queue_.front();
    }

    bool pop_item(QueueItem& item) {
        return queue_.pop(item);
    }

    bool is_queue_empty() {
        return queue_.empty();
    }

    // для избавления от busy loop при использовании асинхронной очереди
    std::atomic<int> futex_flag_{0};// 0 = sleep, 1 = wakeup
    void futex_wake_queue() noexcept {
        futex_flag_.store(1, std::memory_order_release);

        syscall(SYS_futex,
                reinterpret_cast<int*>(&futex_flag_),
                FUTEX_WAKE, 1,
                nullptr, nullptr, 0);
    }

    void futex_wait_queue() noexcept {
        futex_flag_.store(0, std::memory_order_relaxed);

        syscall(SYS_futex,
                reinterpret_cast<int*>(&futex_flag_),
                FUTEX_WAIT, 0,
                nullptr, nullptr, 0);
    }
};


#endif // CLIENT_H
