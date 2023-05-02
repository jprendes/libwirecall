#include <wirecall.hpp>

#include <asio.hpp>

#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

using namespace std::literals;

enum class daytime {
    morning,
    afternoon,
    evening,
};

asio::awaitable<void> client(asio::ip::tcp::socket socket) {
    wirecall::ipc_endpoint<std::string> endpoint{std::move(socket)};

    co_await endpoint.add_method("name", []() {
        return "client"s;
    });

    co_await endpoint.add_method("callback", [](std::string secret) {
        std::cout << "callback received the secret: " << secret << "\n";
    });

    endpoint.run(asio::detached);

    // call a simple method
    auto number = co_await endpoint.call<size_t>("number");
    std::cout << "received number: " << number << "\n";

    // call a method with an argument
    auto greeting = co_await endpoint.call<std::string>("greeting", daytime::afternoon);
    std::cout << "received greeting: " << greeting << "\n";

    // call a method using a callback
    co_await endpoint.call<void>("get_secret", "callback"sv);

    try {
        // call a throwing method
        auto greeting = co_await endpoint.call<std::string>("authorize", "user"sv, "password"sv);
    } catch (std::exception & ex) {
        std::cout << "authorization failed: " << ex.what() << "\n";
    }

    try {
        // call an invalid method
        co_await endpoint.call<void>("invalid");
    } catch (std::exception & ex) {
        std::cout << "invalid method: " << ex.what() << "\n";
    }

    try {
        // call with invalid arguments
        co_await endpoint.call<size_t>("number", 123);
    } catch (std::exception & ex) {
        std::cout << "invalid arguments signature: " << ex.what() << "\n";
    }

    try {
        // call with invalid return type
        co_await endpoint.call<std::string>("number");
    } catch (std::exception & ex) {
        std::cout << "invalid return signature: " << ex.what() << "\n";
    }
}

asio::awaitable<void> server(asio::ip::tcp::socket socket) {
    wirecall::ipc_endpoint<std::string> endpoint{std::move(socket)};

    // a simple method
    co_await endpoint.add_method("number", []() {
        return 42;
    });

    // a method with an argument
    co_await endpoint.add_method("greeting", [&endpoint](daytime t) -> asio::awaitable<std::string> {
        // this method has a nested call to a client's method
        auto name = co_await endpoint.call<std::string>("name");
        switch (t) {
            case daytime::morning: co_return "good morning "s + name;
            case daytime::afternoon: co_return "good afternoon "s + name;
            case daytime::evening: co_return "good evening "s + name;
            default: co_return "hi "s + name;
        }
    });

    // a method using a callback
    co_await endpoint.add_method("get_secret", [&endpoint](std::string callback_name) -> asio::awaitable<void> {
        // use ignore_result since we are not interested in receiving a response
        co_await endpoint.call<wirecall::ignore_result>(callback_name, "a secret"sv);
    });

    // a throwing method
    co_await endpoint.add_method("authorize", [](std::string user, std::string password) {
        throw std::runtime_error("Failed to authorize user \"" + user + "\"");
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

}

int main(void) {
    asio::thread_pool ctx(2);
    asio::co_spawn(ctx, server(), asio::detached);
    asio::co_spawn(ctx, client(), asio::detached);
    ctx.join();
    return 0;
}