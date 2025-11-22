#include "tests.h"
#include "server.h"
#include <boost/mpl/list.hpp>

typedef boost::mpl::list<SimpleServer, MultithreadServer> ServerTypes;


BOOST_AUTO_TEST_CASE_TEMPLATE(TestStopBeforeSend, ServerImpl, ServerTypes) {
    ServerClientFixture<ServerImpl> fixture;
    fixture.client_config_.auto_send = false; // Отключаем автоотправку
    fixture.SetupServerAndClient();

    fixture.server->StartListen();
    fixture.client->Start();

    // Ожидаем подключения
    BOOST_CHECK(fixture.WaitForConnection());

    // Останавливаем до отправки данных
    fixture.client->Stop();

    // Проверяем состояние
    BOOST_CHECK(fixture.WaitForClientState(ClientState::DISCONNECTED));

    // Проверяем статистику - ничего не отправлено
    auto stats = fixture.server->GetClientsStats();
    if (!stats.empty()) {
        BOOST_CHECK_EQUAL(stats[0]->GetSendTotal(), 0);
    }
}

BOOST_AUTO_TEST_CASE_TEMPLATE(TestReconnect, ServerImpl, ServerTypes) {
    ServerClientFixture<ServerImpl> fixture;
    fixture.client_config_.auto_reconnect = true;
    fixture.SetupServerAndClient();

    fixture.server->StartListen();
    fixture.client->Start();

    // Первое подключение
    BOOST_CHECK(fixture.WaitForConnection());
    BOOST_CHECK(fixture.client->IsConnected());

    // Имитируем разрыв
    fixture.client->Stop();
    BOOST_CHECK(fixture.WaitForClientState(ClientState::DISCONNECTED));

    // Повторное подключение
    fixture.client->Start();
    BOOST_CHECK(fixture.WaitForConnection());
    BOOST_CHECK(fixture.client->IsConnected());
}

BOOST_AUTO_TEST_CASE_TEMPLATE(TestAutoSend, ServerImpl, ServerTypes) {
    ServerClientFixture<ServerImpl> fixture;
    fixture.client_config_.auto_send = true;
    fixture.SetupServerAndClient();

    fixture.server->StartListen();
    fixture.client->Start();

    BOOST_CHECK(fixture.WaitForConnection());

    // Ждем некоторое время для автоотправки
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Проверяем, что данные были отправлены
    auto stats = fixture.server->GetClientsStats();
    if (!stats.empty()) {
        BOOST_CHECK_GT(stats[0]->GetRecvTotal(), 0);
    }
}

BOOST_AUTO_TEST_CASE_TEMPLATE(TestClientStates, ServerImpl, ServerTypes) {
    ServerClientFixture<ServerImpl> fixture;
    fixture.SetupServerAndClient();

    // Начальное состояние
    // BOOST_CHECK_EQUAL(fixture.client->ClientState(), ClientState::DISCONNECTED);

    fixture.server->StartListen();
    fixture.client->Start();

    // Проверяем переход в CONNECTING
    BOOST_CHECK(fixture.WaitForClientState(ClientState::CONNECTING, 1000));

    // Проверяем конечное состояние
    BOOST_CHECK(fixture.WaitForClientState(ClientState::WAITING));
    BOOST_CHECK(fixture.client->IsConnected());
}

BOOST_AUTO_TEST_CASE(TestMultithreadServerWorkerDistribution) {
    ServerClientFixture<MultithreadServer> fixture;
    fixture.server_config_.worker_threads = 2;
    fixture.SetupServerAndClient();

    fixture.server->StartListen();

    // Здесь можно тестировать распределение клиентов по воркерам
    auto workers = fixture.server->GetWorkers();
    BOOST_CHECK(workers != nullptr);
    // Дополнительные проверки для многопоточности
}

BOOST_AUTO_TEST_SUITE(ConnectionTests)
BOOST_AUTO_TEST_CASE_TEMPLATE(TestHandshakeSize, ServerImpl, ServerTypes) { /* ... */ }
BOOST_AUTO_TEST_CASE_TEMPLATE(TestReconnect, ServerImpl, ServerTypes) { /* ... */ }
BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(DataFlowTests)
BOOST_AUTO_TEST_CASE_TEMPLATE(TestAutoSend, ServerImpl, ServerTypes) { /* ... */ }
BOOST_AUTO_TEST_CASE_TEMPLATE(TestStopBeforeSend, ServerImpl, ServerTypes) { /* ... */ }
BOOST_AUTO_TEST_SUITE_END()

// int main()
// {
//     // Определяем типы серверов для тестирования
//     // ServerTypes t;
//     // Основная функция уже не нужна - BOOST_TEST_MODULE создает ее автоматически
//     return 0;
// }
