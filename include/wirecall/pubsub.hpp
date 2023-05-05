#pragma once

#include "wirecall/async_channel.hpp"
#include "wirecall/async_mutex.hpp"
#include "wirecall/buffered_socket.hpp"
#include "wirecall/connection.hpp"

#include "wirepump.hpp"

#include <asio/awaitable.hpp>
#include <asio/co_spawn.hpp>
#include <asio/detached.hpp>
#include <asio/generic/stream_protocol.hpp>

#include <functional>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <tuple>
#include <unordered_map>
#include <utility>

namespace wirecall {

namespace details {

template <typename T>
std::string serialize(T && value) {
    std::stringstream ss;
    wirepump::write(ss, std::forward<T>(value));
    return ss.str();
}

template <typename T>
auto deserialize(std::string payload) {
    auto length = payload.length();
    std::stringstream ss(std::move(payload));
    T value;
    wirepump::read(ss, value);
    if (ss.tellg() != length) {
        throw std::runtime_error("Unexpected unused bytes in stream");
    }
    return value;
}

}

template <typename key_type, typename socket_type, template <typename...> typename channel_type>
struct basic_pubsub_endpoint {
  private:
    using callback_type = std::function<asio::awaitable<void>(std::string)>;
    using callback_ptr_type = std::shared_ptr<callback_type>;

    using default_callback_type = std::function<asio::awaitable<void>(key_type, std::string)>;
    using default_callback_ptr_type = std::shared_ptr<default_callback_type>;

    basic_connection<socket_type, basic_async_mutex<channel_type>> m_connection;

    basic_async_mutex<channel_type> m_mutex;
    std::unordered_map<key_type, callback_ptr_type> m_callbacks = {};
    default_callback_ptr_type m_default_callback = nullptr;

  public:
    basic_pubsub_endpoint(socket_type socket)
      : m_connection(std::move(socket))
      , m_mutex(m_connection.get_executor())
    {}

    auto get_executor() {
        return m_connection.get_executor();
    }

    template <typename... Args>
    asio::awaitable<void> publish(key_type key, Args&&... args) {
        auto payload = details::serialize(std::make_tuple(std::forward<Args>(args)...));
        co_await m_connection.send(std::tuple{key, std::move(payload)});
    }

    template <typename... Args>
    asio::awaitable<void> subscribe(key_type key, std::function<asio::awaitable<void>(Args...)> f) {
        auto lock = co_await m_mutex.lock();
        auto f_ptr = std::make_shared<callback_type>([f = std::move(f)](std::string payload) -> asio::awaitable<void> {
            co_await std::apply(f, details::deserialize<std::tuple<Args...>>(std::move(payload)));
        });
        m_callbacks.insert_or_assign(std::move(key), std::move(f_ptr));
    }

    template <typename... Args>
    asio::awaitable<void> subscribe(key_type key, std::function<void(Args...)> f) {
        co_await subscribe(std::move(key), [f = std::move(f)](Args... args) -> asio::awaitable<void> {
            co_return f(args...);
        });
    }

    template <typename F>
    asio::awaitable<void> subscribe(key_type key, F && f) {
        co_await subscribe(std::move(key), std::function{std::forward<F>(f)});
    }

    template <typename... Args>
    void subscribe_default(std::function<asio::awaitable<void>(key_type, Args...)> f) {
        m_default_callback = std::make_shared<default_callback_type>([f = std::move(f)](key_type key, std::string payload) -> asio::awaitable<void> {
            co_await std::apply(f, std::tuple_cat(std::make_tuple(std::move(key)), details::deserialize<std::tuple<Args...>>(std::move(payload))));
        });
    }

    template <typename... Args>
    void subscribe_default(std::function<void(key_type, Args...)> f) {
        subscribe_default([f = std::move(f)](key_type key, Args... args) -> asio::awaitable<void> {
            co_return f(std::move(key), args...);
        });
    }

    template <typename F>
    void subscribe_default(F && f) {
        subscribe_default(std::function{std::forward<F>(f)});
    }

    asio::awaitable<void> unsubscribe(key_type key) {
        auto lock = co_await m_mutex.lock();
        m_callbacks.erase(key);
    }

    void unsubscribe_default(key_type key) {
        m_default_callback = nullptr;
    }

    asio::awaitable<void> run() {
        while (m_connection.is_open()) {
            auto [key, payload] =
                co_await m_connection.template receive<std::tuple<key_type, std::string>>();

            asio::co_spawn(
                m_connection.get_executor(),
                handle_request(std::move(key), std::move(payload)),
                asio::detached
            );
        }
    }

    template <typename token_type>
    auto run(token_type && token) {
        return asio::co_spawn(get_executor(), run(), std::forward<token_type>(token));
    }

    auto is_open() const {
        return m_connection.is_open();
    }

    auto close() {
        return m_connection.close();
    }

  private:
    asio::awaitable<void> handle_request(key_type key, std::string payload) {
        try {
            callback_ptr_type callback = nullptr;
            
            {
                auto lock = co_await m_mutex.lock();
                auto it = m_callbacks.find(key);
                if (it != m_callbacks.end()) callback = it->second;
            }

            if (callback) {
                co_await (*callback)(std::move(payload));
            } else if (auto default_callback = m_default_callback; default_callback) {
                co_await (*default_callback)(std::move(key), std::move(payload));
            }
        } catch (...) {
            // signature missmatch ?
            // anyway, there's not much to with with errors here
        }
    }
};

template <typename key_type>
using pubsub_endpoint = basic_pubsub_endpoint<key_type, buffered_socket<asio::generic::stream_protocol::socket>, async_channel>;

}
