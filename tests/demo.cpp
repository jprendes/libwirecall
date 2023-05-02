#include <wirecall.hpp>

#include <asio.hpp>

#include <iostream>
#include <string>
#include <utility>

asio::awaitable<void> client(asio::ip::tcp::socket socket) {
    wirecall::ipc_endpoint<std::string> endpoint{std::move(socket)};

    endpoint.run(asio::detached);

    auto result = co_await endpoint.call<int>("sum", 20, 22);
    std::cout << "20 + 22 = " << result << "\n";
}

asio::awaitable<void> server(asio::ip::tcp::socket socket) {
    wirecall::ipc_endpoint<std::string> endpoint{std::move(socket)};

    co_await endpoint.add_method("sum", [](int a, int b) -> int {
        return a + b;
    });

    co_await endpoint.run();
}

asio::awaitable<void> client() {
    auto ctx = co_await asio::this_coro::executor;
    asio::ip::tcp::endpoint ep(asio::ip::make_address("127.0.0.1"), 5678);
    asio::ip::tcp::socket socket{ctx, ep.protocol()};
    co_await socket.async_connect(ep, asio::use_awaitable);
    co_await client(std::move(socket));
}

asio::awaitable<void> server() {
    auto ctx = co_await asio::this_coro::executor;
    asio::ip::tcp::endpoint ep(asio::ip::make_address("127.0.0.1"), 5678);
    asio::ip::tcp::acceptor acceptor(ctx, ep);
    asio::ip::tcp::socket socket = co_await acceptor.async_accept(asio::use_awaitable);
    co_await server(std::move(socket));
}

int main(void) {
    asio::thread_pool ctx(2);
    asio::co_spawn(ctx, server(), asio::detached);
    asio::co_spawn(ctx, client(), asio::detached);
    ctx.join();
    return 0;
}