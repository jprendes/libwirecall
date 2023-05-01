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

struct any_socket {
  private:
    struct impl_base {
        virtual asio::awaitable<std::string> read(size_t n = 1024) = 0;
        virtual asio::awaitable<void> write(std::string_view const &) = 0;
        virtual bool is_open() const = 0;
        virtual void close() = 0;
        virtual void cancel() = 0;
        virtual asio::any_io_executor get_executor() = 0;
        virtual ~impl_base() {}
    };

    template <typename socket_type>
    struct impl : public impl_base {
        socket_type m_socket;
        impl(socket_type socket) : m_socket{std::move(socket)} {}
        asio::awaitable<std::string> read(size_t n) override {
            std::string c(n, '\0');
            auto m = co_await m_socket.async_read_some(asio::buffer(c), asio::use_awaitable);
            c.resize(m);
            co_return c;
        }
        asio::awaitable<void> write(std::string_view const & c) override {
            co_await asio::async_write(m_socket, asio::buffer(c), asio::use_awaitable);
        }
        bool is_open() const override { return m_socket.is_open(); }
        void close() override { m_socket.close(); }
        void cancel() override { m_socket.cancel(); }
        asio::any_io_executor get_executor() override { return m_socket.get_executor(); }
    };

  private:
    std::unique_ptr<impl_base> m_impl;
    std::stringstream m_write_buffer;
    std::stringstream m_read_buffer;

  public:
    template <typename socket_type>
        requires requires (socket_type socket, uint8_t c) {
            { wirepump::read(socket, c) } -> std::same_as<asio::awaitable<void>>;
            { wirepump::write(socket, c) } -> std::same_as<asio::awaitable<void>>;
            { socket.is_open() } -> std::same_as<bool>;
            { socket.close() } -> std::same_as<void>;
            { socket.cancel() } -> std::same_as<void>;
        }
    any_socket(socket_type socket) : m_impl{std::make_unique<impl<socket_type>>(std::move(socket))} {}

    asio::awaitable<void> read(uint8_t & c) {
        c = m_read_buffer.get();
        while (m_read_buffer.eof()) {
            m_read_buffer.str(co_await m_impl->read());
            m_read_buffer.clear();
            c = m_read_buffer.get();
        }
    }
    asio::awaitable<void> write(uint8_t const & c) {
        m_write_buffer.put(c);
        co_return;
    }
    asio::awaitable<void> flush() {
        co_await m_impl->write(m_write_buffer.str());
        m_write_buffer.str("");
    }
    bool is_open() const { return m_impl->is_open(); }
    void close() { m_impl->close(); }
    void cancel() { m_impl->cancel(); }
    auto get_executor() { return m_impl->get_executor(); }
};

}

template <>
struct wirepump::read_impl<wirecall::any_socket, uint8_t> {
    static auto read(wirecall::any_socket & socket, uint8_t & c) -> asio::awaitable<void> {
        co_await socket.read(c);
    }
};

template <>
struct wirepump::write_impl<wirecall::any_socket, uint8_t> {
    static auto write(wirecall::any_socket & socket, uint8_t const & c) -> asio::awaitable<void> {
        co_await socket.write(c);
    }
};
