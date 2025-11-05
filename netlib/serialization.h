#ifndef SERIALIZATION_H
#define SERIALIZATION_H

#include "const.h"
#include <cerrno>
#include <cstdint>
#include <sys/socket.h>
#include <sys/types.h>
#include <array>

enum class MessageType : uint8_t {
    AUTH_REQUEST = 1,
    AUTH_RESPONSE = 2,
    DATA_PKT = 3
};

#pragma pack(push, 1)
struct AuthRequest {
    MessageType type = MessageType::AUTH_REQUEST;
    std::array<uint8_t, 16> client_uuid;
};
struct AuthResponse {
    MessageType type = MessageType::AUTH_RESPONSE;
    std::array<uint8_t, 16> client_uuid;
    uint64_t restore_seq_num = 0;
};
struct DataPktHeader {
    MessageType type = MessageType::DATA_PKT;
    uint64_t seq_num;
    uint32_t data_size;
    //char* data; after header (but here only header)
};
#pragma pack(pop)

// Результат парсинга
struct ParsedMessage {
    MessageType type;
    int size_header;
    union {
        AuthRequest auth_request;
        AuthResponse auth_response;
        DataPktHeader packet_header;
    };
    // char* payload = nullptr;
    std::vector<char> packet_data;
};


// bool peekHeader(int sock, char* data, size_t size) {
//     size_t totalReceived = 0;
//     while (totalReceived < size) {
//         ssize_t received = recv(sock, data + totalReceived, size - totalReceived, MSG_PEEK);
//         if (received <= 0) {
//             return false;
//         }
//         totalReceived += received;
//     }
//     return true;
// }



/*
 * чтение из сокета, буферизируем данные, пока не вычитаем весь пакет.
 * используется сразу после конекта или в основном потоке чтения.
 * сокетом не владеем, только для recv !!!
 *
 *TODO:
 * MSG_PEEK?
 * обработка не поддерживаемого типа
 * timeout коннектов - надо отбрасывать на авторизации
 */

//может переписать на connection handler ???
class MessageParser {
private:
    int sockfd_;
    std::vector<char> recv_buffer_;
    size_t parsed_bytes_ = 0;
    bool is_timeout_ = false;

    bool sendAll(const int sockfd, const std::vector<char>& data){
        size_t total_sent = 0;
        const char* ptr = data.data();
        size_t remaining = data.size();

        while (remaining > 0) {
            ssize_t sent = send(sockfd, ptr + total_sent, remaining, MSG_NOSIGNAL);
            if (sent <= 0) {
                return false;
            }
            total_sent += static_cast<size_t>(sent);
            remaining -= static_cast<size_t>(sent);
        }

        std::cout << "send header:" << total_sent << std::endl;
        return true;
    }

    bool sendPacketPayload(const int sockfd, const std::vector<char>& header, char* data, int size){
        sendAll(sockfd, header);
        size_t total_sent = 0;
        size_t remaining = size;
        while (remaining > 0) {
            ssize_t sent = send(sockfd, data + total_sent, remaining, MSG_NOSIGNAL);
            if (sent <= 0) {
                return false;
            }
            total_sent += static_cast<size_t>(sent);
            remaining -= static_cast<size_t>(sent);
        }
        std::cout << "send data:" << total_sent << std::endl;

        return true;
    }

public:
    MessageParser(int sockfd, size_t size_buff) : sockfd_(sockfd) {
        // todo может только тогда когда используется
        // min for detect type for protect
        recv_buffer_.reserve(std::max(size_buff, sizeof(MessageType)));
        // setSocketTimeout(sockfd_, 1);
    }

    // Основной метод: читает и парсит одно сообщение
    bool readMessage(ParsedMessage& result, bool wait_timeout = true) {

        while (true) {
            if (!readAvailable()) {
                return false;
            }
            if (is_timeout_){
                std::cout << "timeout" << std::endl;
                is_timeout_ = false;
                if (wait_timeout){
                    continue;
                }
                return false;
            }

            // Пытаемся распарсить из буфера
            if (tryParseMessage(result)) {
                return true;
            }
        }
    }

    // Парсинг сообщения из буфера
    bool tryParseMessage(ParsedMessage& result) {
        // need min msg - MessageType
        if (recv_buffer_.size() - parsed_bytes_ < sizeof(MessageType)) {
            return false;
        }
        MessageType type = *reinterpret_cast<MessageType*>(
            recv_buffer_.data() + parsed_bytes_);


        switch (type) {
        case MessageType::AUTH_REQUEST:
            return parseAuthRequest(result);
        case MessageType::AUTH_RESPONSE:
            return parseAuthResponse(result);
        case MessageType::DATA_PKT:
            return parseDataPacket(result);
        default:
            // Неизвестный тип - пропускаем байт
            parsed_bytes_++;
            return false;
        }
    }

    void sendAuthResponce(ParsedMessage& msg, const std::array<uint8_t, 16> uuid, const uint64_t& restore_seq_num){
        msg.auth_response.type = MessageType::AUTH_RESPONSE;
        msg.auth_response.client_uuid = uuid;
        msg.auth_response.restore_seq_num = 0;
        msg.size_header = sizeof(AuthResponse);

        //sendAuthResponce
        std::vector<char> resp(msg.size_header);
        memcpy(resp.data(), &msg.auth_response, msg.size_header);
        sendAll(sockfd_, resp);
    }

    void sendAuthRequest(ParsedMessage& msg, const std::array<uint8_t, 16> uuid){
        msg.auth_request.type = MessageType::AUTH_REQUEST;
        msg.auth_request.client_uuid = uuid;
        msg.size_header = sizeof(AuthRequest);

        //sendAuthResponce
        std::vector<char> resp(msg.size_header);
        memcpy(resp.data(), &msg.auth_request, msg.size_header);
        sendAll(sockfd_, resp);
    }

    void sendDataPkt(ParsedMessage& msg, uint64_t seq_num, char* data, int data_size){
        msg.packet_header.type = MessageType::DATA_PKT;
        msg.packet_header.seq_num = seq_num;
        msg.packet_header.data_size = data_size;
        msg.size_header = sizeof(DataPktHeader) + data_size;

        //sendAuthResponce
        std::vector<char> resp(sizeof(DataPktHeader));
        memcpy(resp.data(), &msg.packet_header, sizeof(DataPktHeader));
        sendAll(sockfd_, resp);
        sendPacketPayload(sockfd_, resp, data, data_size);
    }


private:
    bool parseAuthRequest(ParsedMessage& result) {
        const size_t required_size = sizeof(AuthRequest);
        if (recv_buffer_.size() - parsed_bytes_ < required_size) {
            return false;
        }

        result.type = MessageType::AUTH_REQUEST;
        std::memcpy(&result.auth_request,
                    recv_buffer_.data() + parsed_bytes_,
                    required_size);
        parsed_bytes_ += required_size;

        return true;
    }

    bool parseAuthResponse(ParsedMessage& result) {
        const size_t required_size = sizeof(AuthResponse);
        if (recv_buffer_.size() - parsed_bytes_ < required_size) {
            return false;
        }

        result.type = MessageType::AUTH_RESPONSE;
        std::memcpy(&result.auth_response,
                    recv_buffer_.data() + parsed_bytes_,
                    required_size);
        parsed_bytes_ += required_size;

        return true;
    }

    bool parseDataPacket(ParsedMessage& result) {
        // Сначала читаем заголовок
        const size_t required_size = sizeof(DataPktHeader);
        if (recv_buffer_.size() - parsed_bytes_ < required_size) {
            return false;
        }

        DataPktHeader header;
        std::memcpy(&header,
                    recv_buffer_.data() + parsed_bytes_,
                    required_size);

        // Проверяем, есть ли полные данные
        size_t total_size = required_size + header.data_size;
        if (recv_buffer_.size() - parsed_bytes_ < total_size) {
            return false;
        }

        // Копируем заголовок и данные
        result.type = MessageType::DATA_PKT;
        result.packet_header = header;
        result.packet_data.resize(header.data_size);

        std::memcpy(result.packet_data.data(),
                    recv_buffer_.data() + parsed_bytes_ + required_size,
                    header.data_size);

        parsed_bytes_ += total_size;
        return true;
    }

    // Чтение данных из сокета
    bool readAvailable() {
        // Освобождаем место если нужно
        // if (parsed_bytes_ > recv_buffer_.size() / 2) {
        //     compactBuffer();
        // }

        // Читаем в доступное место
        size_t available_space = recv_buffer_.capacity() - recv_buffer_.size();
        if (available_space < 1024) {
            recv_buffer_.reserve(recv_buffer_.capacity() * 2);
            available_space = recv_buffer_.capacity() - recv_buffer_.size();
        }

        std::vector<char> temp_buf(1024);
        ssize_t received = recv(sockfd_, temp_buf.data(), temp_buf.size(), 0);
        // std::cout << sockfd_ << " recv:" << received << " " << temp_buf.size() << " " << errno << std::endl;
        if (received > 0) {
            recv_buffer_.insert(recv_buffer_.end(),
                                temp_buf.data(),
                                temp_buf.data() + received);
            return true;
        } else if (received == 0) {
            return false; // Connection closed
        } else {
            //timeout
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                is_timeout_ = true;
                return true;
            }
            return false; // Real error
        }
    }

    // void compactBuffer() {
    //     if (parsed_bytes_ > 0) {
    //         if (parsed_bytes_ < recv_buffer_.size()) {
    //             std::memmove(recv_buffer_.data(),
    //                          recv_buffer_.data() + parsed_bytes_,
    //                          recv_buffer_.size() - parsed_bytes_);
    //         }
    //         recv_buffer_.resize(recv_buffer_.size() - parsed_bytes_);
    //         parsed_bytes_ = 0;
    //     }
    // }

    static void setSocketTimeout(int sockfd, int seconds) {
        struct timeval tv;
        tv.tv_sec = seconds;
        tv.tv_usec = 0;
        setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    }
};

#endif // SERIALIZATION_H
