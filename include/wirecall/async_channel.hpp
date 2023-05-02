#pragma once

#include <asio/awaitable.hpp>
#include <asio/error_code.hpp>
#include <asio/experimental/concurrent_channel.hpp>
#include <asio/use_awaitable.hpp>

#include <concepts>
#include <memory>
#include <optional>
#include <tuple>
#include <utility>

namespace wirecall {

template <template<typename, typename...> typename asio_channel_template, typename... Tp>
struct basic_async_channel {
  private:
    inline static asio::error_code m_success{0, asio::system_category()};

    using asio_channel_type = asio_channel_template<void(asio::error_code, Tp...)>;
    using return_type = decltype(std::declval<asio_channel_type>().async_receive(asio::use_awaitable).await_resume());

    std::shared_ptr<asio_channel_type> m_channel;

  public:
    template <typename executor_type>
    basic_async_channel(executor_type const & executor, size_t size = 1)
      : m_channel{std::make_shared<asio_channel_type>(executor, size)}
    {}

    auto get_executor() {
        return m_channel.get_executor();
    }

    template <typename... Up>
    bool try_send(Up&&... values) {
        return m_channel->try_send(m_success, std::forward<Up>(values)...);
    }

    template <typename... Up>
    asio::awaitable<void> async_send(Up&&... values) {
        co_await m_channel->async_send(m_success, std::forward<Up>(values)...);
    }

    void cancel() {
        m_channel->cancel();
    }

    auto try_receive() {
        if constexpr (std::same_as<return_type, void>) {
            return m_channel->try_receive([](auto&&...){});
        } else {
            std::optional<return_type> result;
            m_channel->try_receive([this, &result](asio::error_code, auto&&... values){
                result = return_type(std::forward<decltype(values)>(values)...);
            });
            return result;
        }
    }

    asio::awaitable<return_type> async_receive() {
        co_return co_await m_channel->async_receive(asio::use_awaitable);
    }
};

template <typename... Tp>
using async_channel = basic_async_channel<asio::experimental::concurrent_channel, Tp...>;

}
