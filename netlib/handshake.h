#ifndef HANDSHAKE_H
#define HANDSHAKE_H

#include "const.h"
#include <array>

#pragma pack(push,1)
struct ClientHiMsg {
    std::array<uint8_t, 16> uuid;
};
enum ClientMode : uint8_t {
    ERRUUID = 0,
    SEND    = 1
};
struct ServerAnsHiMsg {
    std::array<uint8_t, 16> client_uuid;
    ClientMode client_mode;
};
#pragma pack(pop)
#pragma once

class HandshakeWrapper {
public:
    template<typename T>
    static bool sendStruct(int fd, const T& obj, int timeout_ms)
    {
        std::vector<uint8_t> payload;
        serialize(obj, payload);

        // size prefix
        uint32_t size = htonl((uint32_t)payload.size());

        iovec iov[2]{};
        iov[0].iov_base = &size;
        iov[0].iov_len  = sizeof(size);
        iov[1].iov_base = payload.data();
        iov[1].iov_len  = payload.size();

        msghdr msg{};
        msg.msg_iov = iov;
        msg.msg_iovlen = 2;

        SocketTimeoutGuard tg(fd, timeout_ms);

        ssize_t sent = sendmsg(fd, &msg, 0);
        if (sent < 0) return false;

        size_t want = sizeof(size) + payload.size();
        if (sent == want) return true;

        size_t remain = want - sent;
        uint8_t* tail = payload.data() + (payload.size() - remain);
        return sendAll(fd, tail, remain);
    }

    template<typename T>
    static bool recvStruct(int fd, T& obj, int timeout_ms)
    {
        SocketTimeoutGuard tg(fd, timeout_ms);

        uint32_t net_size;
        if (!recvAll(fd, &net_size, sizeof(net_size))) return false;

        uint32_t size = ntohl(net_size);

        std::vector<uint8_t> buf(size);
        if (!recvAll(fd, buf.data(), size)) return false;

        return deserialize(buf.data(), buf.size(), obj);
    }

    template<typename T>
    static void serialize(const T& obj, std::vector<uint8_t>& out)
    {
        out.resize(sizeof(T));
        memcpy(out.data(), &obj, sizeof(T));
    }

    template<typename T>
    static bool deserialize(const uint8_t* data, size_t size, T& out)
    {
        if (size != sizeof(T)) return false;
        memcpy(&out, data, sizeof(T));
        return true;
    }

private:
    static bool sendAll(int fd, const void* data, size_t size)
    {
        const uint8_t* p = (const uint8_t*)data;
        size_t left = size;

        while (left > 0) {
            ssize_t s = send(fd, p, left, 0);
            if (s <= 0) return false;
            p += s;
            left -= s;
        }
        return true;
    }

    static bool recvAll(int fd, void* data, size_t size)
    {
        uint8_t* p = (uint8_t*)data;
        size_t left = size;

        while (left > 0) {
            ssize_t r = recv(fd, p, left, 0);
            if (r <= 0) return false;
            p += r;
            left -= r;
        }
        return true;
    }


    class SocketTimeoutGuard {
    public:
        SocketTimeoutGuard(int fd, int timeout_ms)
            : fd_(fd)
        {
            socklen_t sl = sizeof(old_rcv_);
            getsockopt(fd_, SOL_SOCKET, SO_RCVTIMEO, &old_rcv_, &sl);
            getsockopt(fd_, SOL_SOCKET, SO_SNDTIMEO, &old_snd_, &sl);

            timeval tv{};
            tv.tv_sec = timeout_ms / 1000;
            tv.tv_usec = (timeout_ms % 1000) * 1000;

            setsockopt(fd_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
            setsockopt(fd_, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
        }

        ~SocketTimeoutGuard()
        {
            setsockopt(fd_, SOL_SOCKET, SO_RCVTIMEO, &old_rcv_, sizeof(old_rcv_));
            setsockopt(fd_, SOL_SOCKET, SO_SNDTIMEO, &old_snd_, sizeof(old_snd_));
        }

    private:
        int fd_;
        timeval old_rcv_{};
        timeval old_snd_{};
    };
};



#endif // HANDSHAKE_H
