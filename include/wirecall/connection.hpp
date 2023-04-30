#pragma once

#include "wirecall/any_socket.hpp"
#include "wirecall/async_mutex.hpp"

#include "wirepump.hpp"

#include <asio/awaitable.hpp>

#include <concepts>
#include <utility>

namespace wirecall {

template <typename socket_type, typename mutex_type>
struct basic_connection {
  private:
    socket_type m_socket;
    mutex_type read_mutex;
    mutex_type write_mutex;

  public:
    basic_connection(socket_type socket)
      : m_socket{std::move(socket)}
      , read_mutex{m_socket.get_executor()}
      , write_mutex{m_socket.get_executor()}
    {}
  
  public:
    auto get_executor() {
        return m_socket.get_executor();
    }

    template <typename T>
    asio::awaitable<void> send(T const & msg) {
        auto lock = co_await write_mutex.lock();
        co_await wirepump::write(m_socket, msg);
        if constexpr (requires (socket_type socket) {
            { socket.flush() } -> std::same_as<asio::awaitable<void>>;
        }) {
            co_await m_socket.flush();
        }
    }

    template <typename T>
    asio::awaitable<void> receive(T & msg) {
        auto lock = co_await read_mutex.lock();
        co_await wirepump::read(m_socket, msg);
    }

    template <typename T>
    asio::awaitable<T> receive() {
        T msg{};
        co_await receive(msg);
        co_return msg;
    }

    auto is_open() const {
        return m_socket.is_open();
    }

    auto close() {
        m_socket.close();
        m_socket.cancel();
    }
};

using connection = basic_connection<any_socket, async_mutex>;

}
