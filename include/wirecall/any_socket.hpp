#pragma once

#include "wirepump.hpp"

#include <asio/any_io_executor.hpp>
#include <asio/awaitable.hpp>
#include <asio/write.hpp>

#include <concepts>
#include <cstdint>
#include <memory>
#include <span>
#include <sstream>
#include <utility>

namespace wirecall {

struct any_socket {
  private:
    struct impl_base {
        virtual asio::awaitable<void> read(uint8_t &) = 0;
        virtual asio::awaitable<void> write(std::span<uint8_t const> const &) = 0;
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
        asio::awaitable<void> read(uint8_t & c) override {
            co_await wirepump::read(m_socket, c);
        }
        asio::awaitable<void> write(std::span<uint8_t const> const & c) override {
            co_await asio::async_write(m_socket, asio::buffer(c.data(), c.size()), asio::use_awaitable);
        }
        bool is_open() const override { return m_socket.is_open(); }
        void close() override { m_socket.close(); }
        void cancel() override { m_socket.cancel(); }
        asio::any_io_executor get_executor() override { return m_socket.get_executor(); }
    };

  private:
    std::unique_ptr<impl_base> m_impl;
    std::stringstream m_stream;

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
        co_await m_impl->read(c);
    }
    asio::awaitable<void> write(uint8_t const & c) {
        wirepump::write(m_stream, c);
        co_return;
    }
    asio::awaitable<void> flush() {
        auto data = m_stream.str();
        m_stream.str("");
        co_await m_impl->write({(uint8_t const *)data.data(), data.size()});
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
