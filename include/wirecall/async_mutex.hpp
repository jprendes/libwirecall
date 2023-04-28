#pragma once

#include "wirecall/async_channel.hpp"

#include <asio/awaitable.hpp>

#include <optional>
#include <utility>

namespace wirecall {

template <template<typename...> typename channel_type>
struct basic_async_mutex {
  private:
    channel_type<> m_channel;

  public:
    struct [[nodiscard]] async_lock {
      protected:
        basic_async_mutex * m_mutex;
        async_lock(basic_async_mutex * mutex) : m_mutex(mutex) {}
        friend struct basic_async_mutex;
    
      public:
        async_lock() = delete;
        async_lock(async_lock &) = delete;
        async_lock(async_lock const &) = delete;
        async_lock(async_lock && other) {
            m_mutex = other.m_mutex;
            other.m_mutex = nullptr;
        }

        ~async_lock() {
            std::move(*this).unlock();
        }

        void unlock() && {
            if (m_mutex) m_mutex->unlock();
            m_mutex = nullptr;
        }

        bool owned_by(basic_async_mutex const & owner) const {
            return m_mutex == &owner;
        }
    };

    template <typename executor_type>
    basic_async_mutex(executor_type const & executor)
      : m_channel{executor}
    {
        unlock();
    }

    std::optional<async_lock> try_lock() {
        if (m_channel.try_receive()) {
            return async_lock(this);
        }
        return std::nullopt;
    }

    asio::awaitable<async_lock> lock() {
        co_await m_channel.async_receive();
        co_return async_lock(this);
    }

  private:
    void unlock() {
        auto tf = m_channel.try_send();
        assert(tf);
    }

    friend struct async_lock;
};

using async_mutex = basic_async_mutex<async_channel>;

}
