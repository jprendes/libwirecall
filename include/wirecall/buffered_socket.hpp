#pragma once

#include "wirepump.hpp"

#include <asio/any_io_executor.hpp>
#include <asio/awaitable.hpp>
#include <asio/buffer.hpp>
#include <asio/use_awaitable.hpp>
#include <asio/write.hpp>

#include <concepts>
#include <cstdint>
#include <memory>
#include <sstream>
#include <string_view>
#include <string>
#include <utility>

namespace wirecall {

template <typename socket_type>
    requires requires (socket_type socket, uint8_t c) {
        { wirepump::read(socket, c) } -> std::same_as<asio::awaitable<void>>;
        { wirepump::write(socket, c) } -> std::same_as<asio::awaitable<void>>;
        { socket.is_open() } -> std::same_as<bool>;
        { socket.close() } -> std::same_as<void>;
        { socket.cancel() } -> std::same_as<void>;
    }
struct buffered_socket {
  private:
    socket_type m_socket;
    std::stringstream m_write_buffer;
    std::stringstream m_read_buffer;

  public:
    template <typename other_socket_type>
        requires requires (other_socket_type socket) {
            socket_type{std::move(socket)};
        }
    buffered_socket(other_socket_type socket) : m_socket{std::move(socket)} {}

    asio::awaitable<void> read(uint8_t & c) {
        c = m_read_buffer.get();
        while (m_read_buffer.eof()) {
            std::string buffer(1024, '\0');
            auto n = co_await m_socket.async_read_some(asio::buffer(buffer.data(), buffer.size()), asio::use_awaitable);
            buffer.resize(n);
            m_read_buffer.str(std::move(buffer));
            m_read_buffer.clear();
            c = m_read_buffer.get();
        }
    }
    asio::awaitable<void> write(uint8_t const & c) {
        m_write_buffer.put(c);
        co_return;
    }
    asio::awaitable<void> flush() {
        std::string buffer = m_write_buffer.str();
        co_await asio::async_write(m_socket, asio::buffer(buffer.data(), buffer.size()), asio::use_awaitable);
        m_write_buffer.str("");
    }
    bool is_open() const { return m_socket.is_open(); }
    void close() { m_socket.close(); }
    void cancel() { m_socket.cancel(); }
    auto get_executor() { return m_socket.get_executor(); }
};

}

template <typename socket_type>
struct wirepump::read_impl<wirecall::buffered_socket<socket_type>, uint8_t> {
    static auto read(wirecall::buffered_socket<socket_type> & socket, uint8_t & c) -> asio::awaitable<void> {
        co_await socket.read(c);
    }
};

template <typename socket_type>
struct wirepump::write_impl<wirecall::buffered_socket<socket_type>, uint8_t> {
    static auto write(wirecall::buffered_socket<socket_type> & socket, uint8_t const & c) -> asio::awaitable<void> {
        co_await socket.write(c);
    }
};
