#ifndef SERIALIZATION_H
#define SERIALIZATION_H

#include "const.h"
#include <cerrno>
#include <cstdint>
#include <sys/socket.h>
#include <sys/types.h>

enum class MessageType : uint8_t {
    AUTH_REQUEST = 1,
    AUTH_RESPONSE,
    DATA_PKT
};

#pragma pack(push, 1)
struct AuthRequest {
    MessageType type = MessageType::AUTH_REQUEST;
    string client_uuid;
};
struct AuthResponse {
    MessageType type = MessageType::AUTH_RESPONSE;
    string client_uuid;
    uint64_t restore_seq_num = 0;
};
struct PacketHeader {
    MessageType type = MessageType::DATA_PKT;
    uint64_t seq_num;
    uint32_t data_size;
    char* data;
};
#pragma pack(pop)


//SocketReader
class serialization
{
public:
    serialization();


    void serialize(int sock){

    }
};

#endif // SERIALIZATION_H
