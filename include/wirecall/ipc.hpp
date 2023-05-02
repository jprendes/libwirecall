#pragma once

#include "wirecall/async_channel.hpp"
#include "wirecall/async_mutex.hpp"
#include "wirecall/connection.hpp"
#include "wirecall/pubsub.hpp"

#include "wirepump.hpp"

#include <asio/awaitable.hpp>

#include <concepts>
#include <cstdint>
#include <exception>
#include <functional>
#include <memory>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <string>
#include <tuple>
#include <utility>
#include <variant>

namespace wirecall {

struct ignore_result {};

class host_error : public std::runtime_error {
  public:
    host_error(std::string_view message)
      : std::runtime_error(std::string{"host error: "}.append(message))
    {}
};

template <typename named_key_type, typename socket_type, template <typename...> typename channel_type>
struct basic_ipc_endpoint {
  private:
    using anonymous_key_type = uint64_t;
    using key_type = std::variant<anonymous_key_type, named_key_type>;

    using method_type = std::function<asio::awaitable<void>(std::string)>;
    using method_ptr_type = std::shared_ptr<method_type>;

    basic_pubsub_endpoint<key_type, socket_type, channel_type> m_pubsub;

    basic_async_mutex<channel_type> m_anonymous_key_mutex;
    std::set<anonymous_key_type> m_anonymous_key_pool = {};
    anonymous_key_type m_next_anonymous_key = 0;

  public:
    basic_ipc_endpoint(socket_type socket)
      : m_pubsub(std::move(socket))
      , m_anonymous_key_mutex{m_pubsub.get_executor()}
    {
        m_pubsub.subscribe_default([this](key_type key, std::optional<key_type> result_key, std::string paylod) -> asio::awaitable<void> {
            if (!result_key) {
                co_return;
            }

            bool success = false;
            std::stringstream message;

            if (key.index() == 1) {
                if constexpr (requires (std::ostream & o, named_key_type k) {
                    o << k;
                }) {
                    message << "Invalid method key `" << std::get<1>(key) << "`";
                } else {
                    message << "Invalid method key";
                }
            }

            co_await m_pubsub.publish(*result_key, false, message.str());
        });
    }

    auto get_executor() {
        return m_pubsub.get_executor();
    }

    template <typename R, typename... Args>
    asio::awaitable<void> add_method(named_key_type named_key, std::function<asio::awaitable<R>(Args...)> f) {
        key_type key{std::in_place_index<1>, std::move(named_key)};

        co_await m_pubsub.subscribe(
            std::move(key),
            [this, f=std::move(f)](std::optional<key_type> result_key, std::string paylod) -> asio::awaitable<void> {
                bool success = true;
                std::string result;

                try {
                    if constexpr (std::same_as<R, void>) {
                        co_await std::apply(f, details::deserialize<std::tuple<Args...>>(paylod));
                    } else {
                        result = details::serialize(co_await std::apply(f, details::deserialize<std::tuple<Args...>>(paylod)));
                    }
                    success = true;
                } catch (std::exception const & ex) {
                    success = false;
                    result = ex.what();
                } catch (...) {
                    success = false;
                    result = "Unknown exception";
                }

                if (result_key) {
                    co_await m_pubsub.publish(*result_key, success, result);
                }
            }
        );
    }

    template <typename R, typename... Args>
    asio::awaitable<void> add_method(named_key_type key, std::function<R(Args...)> f) {
        co_await add_method(std::move(key), [f = std::move(f)](Args... args) -> asio::awaitable<R> {
            co_return f(args...);
        });
    }

    template <typename F>
    asio::awaitable<void> add_method(named_key_type key, F && f) {
        co_await add_method(std::move(key), std::function{std::forward<F>(f)});
    }

    asio::awaitable<void> remove_method(named_key_type key) {
        co_await m_pubsub.unsubscribe({std::in_place_index<1>, std::move(key)});
    }

    template <typename R, typename... Args>
    asio::awaitable<R> call(named_key_type named_key, Args&&... args) {
        key_type key{std::in_place_index<1>, std::move(named_key)};
        std::string payload = details::serialize<std::tuple<Args...>>({std::forward<Args>(args)...});

        if constexpr (std::same_as<R, ignore_result>) {

            co_await m_pubsub.publish(std::move(key), std::optional<key_type>{}, std::move(payload));
            co_return ignore_result{};

        } else {
            key_type result_key = co_await allocate_anonymous_key();

            channel_type<bool, std::string> result_channel{get_executor()};

            co_await m_pubsub.subscribe(result_key, [
                this, result_key, result_channel
            ](bool success, std::string payload) mutable -> asio::awaitable<void> {
                co_await m_pubsub.unsubscribe(result_key);
                co_await release_anonymous_key(result_key);
                result_channel.try_send(success, std::move(payload));
            });

            co_await m_pubsub.publish(std::move(key), std::optional{result_key}, std::move(payload));

            auto [success, result] = co_await result_channel.async_receive();

            if (!success) {
                throw host_error(std::move(result));
            }

            if constexpr (std::same_as<R, void>) {
                // Deserialize something of size zero
                details::deserialize<std::tuple<>>(std::move(result));
            } else {
                co_return details::deserialize<R>(std::move(result));
            }
        }
    }

    asio::awaitable<void> run() {
        co_await m_pubsub.run();
    }

    template <typename token_type>
    auto run(token_type && token) {
        return asio::co_spawn(get_executor(), run(), std::forward<token_type>(token));
    }

    auto is_open() const {
        return m_pubsub.is_open();
    }

    auto close() {
        return m_pubsub.close();
    }

  private:
    asio::awaitable<key_type> allocate_anonymous_key() {
        auto lock = co_await m_anonymous_key_mutex.lock();
        anonymous_key_type key;
        if (!m_anonymous_key_pool.empty()) {
            key = m_anonymous_key_pool.extract(m_anonymous_key_pool.begin()).value();
        } else {
            key = m_next_anonymous_key++;
        }
        co_return key_type{std::in_place_index<0>, std::move(key)};
    }

    asio::awaitable<void> release_anonymous_key(key_type key) {
        auto lock = co_await m_anonymous_key_mutex.lock();
        m_anonymous_key_pool.insert(std::get<0>(std::move(key)));
    }
};

template <typename key_type>
using ipc_endpoint = basic_ipc_endpoint<key_type, any_socket, async_channel>;

}
