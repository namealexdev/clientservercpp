#ifndef QUEUE_H
#define QUEUE_H

#include "const.h"

struct Packet {
    uint64_t sec_num;
    int size;
    char* data;
};

class PacketQueue{
    std::mutex mtx_;
    std::queue<Packet> queue_;
    uint64_t sec_num_ = 0;
    std::vector<char> rawbuffer_;

public:
    PacketQueue(){
        rawbuffer_.resize(1024);
    }

    void write_data(char* data, int size){
        // write to buffer
        rawbuffer_.emplace_back(data);

        // try read - serilization
        Packet d;
        d.sec_num = sec_num_++;
        std::lock_guard lock(mtx_);
        queue_.push(std::move(d));
    }
    bool try_get_pkt(Packet& out) {
        std::lock_guard lock(mtx_);
        if (queue_.empty()) return false;
        out = std::move(queue_.front());
        queue_.pop();
        return true;
    }
};

#endif // QUEUE_H
