# wirecall

A small [asio](https://think-async.com/Asio/) based header-only C++ library for inter process communication.

## Installation

```bash
git clone https://github.com/jprendes/libwirecall.git
cd libwirecall
cmake -B ./build/ -G Ninja
cmake --build ./build/
sudo cmake --install ./build/
```

## Getting started

It works out of the box with asio sockets:
```c++
#include <asio.hpp>
#include <asio/experimental/awaitable_operators.hpp>

#include <wirecall.hpp>

#include <iostream>
#include <string>
#include <utility>

asio::awaitable<void> client(asio::ip::tcp::socket socket) {
    wirecall::ipc_endpoint<std::string> endpoint{std::move(socket)};

    asio::co_spawn(
        endpoint.get_executor(),
        endpoint.run(),
        asio::detached
    );

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

asio::awaitable<void> client(asio::io_context & ctx) {
    asio::ip::tcp::endpoint ep(asio::ip::make_address("127.0.0.1"), 5678);
    asio::ip::tcp::socket socket{ctx, ep.protocol()};
    co_await socket.async_connect(ep, asio::use_awaitable);
    co_await client(std::move(socket));
}

asio::awaitable<void> server(asio::io_context & ctx) {
    asio::ip::tcp::endpoint ep(asio::ip::make_address("127.0.0.1"), 5678);
    asio::ip::tcp::acceptor acceptor(ctx, ep);
    asio::ip::tcp::socket socket = co_await acceptor.async_accept(asio::use_awaitable);
    co_await server(std::move(socket));
}

int main(void) {
    using namespace asio::experimental::awaitable_operators;
    asio::io_context ctx(2);
    asio::co_spawn(ctx, server(ctx) && client(ctx), asio::detached);
    ctx.run();
    return 0;
}
```
