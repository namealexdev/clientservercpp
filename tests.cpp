#include "tests.h"
#include "libinclude/netlib.h"
#include <boost/mpl/list.hpp>

typedef boost::mpl::list<SinglethreadFactory, MultithreadFactory> ServerTypes;
typedef boost::mpl::list<SinglethreadFactory, MultithreadFactory> ClientTypes;
// test_packet_parser.cpp
// #define BOOST_TEST_MODULE PacketParserTests
#include <boost/test/unit_test.hpp>
#include "netlib/packetparser.h"

BOOST_AUTO_TEST_SUITE(PacketParserTests)

BOOST_AUTO_TEST_CASE(ParseFullPacket) {
    PacketParser parser;

    // Создаём пакет: 4 байта размер + данные
    uint32_t payload_size = 10;
    uint32_t net_size = htonl(payload_size);
    std::vector<char> packet(4 + payload_size);
    std::memcpy(packet.data(), &net_size, 4);
    std::memset(packet.data() + 4, 'A', payload_size);

    // Парсим за один раз
    int parsed = parser.ParseDataPacket(packet.data(), packet.size());

    BOOST_CHECK_EQUAL(parsed, 14);
    BOOST_CHECK(parser.IsPacketReady());
    BOOST_CHECK_EQUAL(parser.GetPayloadSize(), 10);
    BOOST_CHECK_EQUAL(std::memcmp(parser.GetPayloadData(), "AAAAAAAAAA", 10), 0);
}

BOOST_AUTO_TEST_CASE(ParseFragmentedHeader) {
    PacketParser parser;

    uint32_t payload_size = 5;
    uint32_t net_size = htonl(payload_size);
    std::vector<char> full_packet(9);
    std::memcpy(full_packet.data(), &net_size, 4);
    std::memset(full_packet.data() + 4, 'B', 5);

    // Первый вызов: только 2 байта заголовка
    int parsed1 = parser.ParseDataPacket(full_packet.data(), 2);
    BOOST_CHECK_EQUAL(parsed1, 2);
    BOOST_CHECK(!parser.IsPacketReady());

    // Второй вызов: остаток заголовка + данные
    int parsed2 = parser.ParseDataPacket(full_packet.data() + 2, 7);
    BOOST_CHECK_EQUAL(parsed2, 7);
    BOOST_CHECK(parser.IsPacketReady());
    BOOST_CHECK_EQUAL(parser.GetPayloadSize(), 5);
}

BOOST_AUTO_TEST_CASE(ParseMultiplePackets) {
    PacketParser parser;

    // Два пакета подряд
    std::vector<char> buffer;
    for (int i = 0; i < 2; i++) {
        uint32_t size = 3;
        uint32_t net_size = htonl(size);
        buffer.insert(buffer.end(), (char*)&net_size, (char*)&net_size + 4);
        buffer.insert(buffer.end(), 3, 'X' + i);
    }

    // Первый пакет
    int parsed1 = parser.ParseDataPacket(buffer.data(), 7);
    BOOST_CHECK_EQUAL(parsed1, 7);
    BOOST_CHECK(parser.IsPacketReady());
    BOOST_CHECK_EQUAL(parser.GetPayloadData()[0], 'X');

    parser.Reset();

    // Второй пакет
    int parsed2 = parser.ParseDataPacket(buffer.data() + 7, 7);
    BOOST_CHECK_EQUAL(parsed2, 7);
    BOOST_CHECK(parser.IsPacketReady());
    BOOST_CHECK_EQUAL(parser.GetPayloadData()[0], 'Y');
}

BOOST_AUTO_TEST_CASE(ParseZeroSizePacket) {
    PacketParser parser;

    uint32_t size = 0;
    uint32_t net_size = htonl(size);

    int parsed = parser.ParseDataPacket((const char*)&net_size, 4);
    BOOST_CHECK_EQUAL(parsed, 4);
    BOOST_CHECK(parser.IsPacketReady());
    BOOST_CHECK_EQUAL(parser.GetPayloadSize(), 0);
}

BOOST_AUTO_TEST_CASE(ResetAndReuse) {
    PacketParser parser;

    // Первый пакет
    uint32_t size1 = 5;
    uint32_t net_size1 = htonl(size1);
    std::vector<char> packet1(9);
    std::memcpy(packet1.data(), &net_size1, 4);
    std::memset(packet1.data() + 4, 'A', 5);

    parser.ParseDataPacket(packet1.data(), 9);
    BOOST_CHECK(parser.IsPacketReady());

    // Reset
    parser.Reset();
    BOOST_CHECK(!parser.IsPacketReady());
    BOOST_CHECK_EQUAL(parser.GetPayloadSize(), 0);

    // Второй пакет
    uint32_t size2 = 3;
    uint32_t net_size2 = htonl(size2);
    std::vector<char> packet2(7);
    std::memcpy(packet2.data(), &net_size2, 4);
    std::memset(packet2.data() + 4, 'B', 3);

    parser.ParseDataPacket(packet2.data(), 7);
    BOOST_CHECK(parser.IsPacketReady());
    BOOST_CHECK_EQUAL(parser.GetPayloadSize(), 3);
}

BOOST_AUTO_TEST_SUITE_END()

