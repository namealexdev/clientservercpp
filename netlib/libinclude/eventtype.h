#ifndef EVENTTYPE_H
#define EVENTTYPE_H

enum class EventType {
    ClientConnected,
    ClientDisconnected,
    DataReceived,

    WriteReady
};
struct DataReceived{
    int size;
    char* data;
};

#endif // EVENTTYPE_H
