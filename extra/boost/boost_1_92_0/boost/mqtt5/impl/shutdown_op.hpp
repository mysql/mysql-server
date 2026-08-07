//
// Copyright (c) 2023-2026 Ivica Siladic, Bruno Iljazovic, Korina Simicevic
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MQTT5_SHUTDOWN_OP_HPP
#define BOOST_MQTT5_SHUTDOWN_OP_HPP

#include <boost/mqtt5/types.hpp>

#include <boost/mqtt5/detail/async_traits.hpp>
#include <boost/mqtt5/detail/shutdown.hpp>

#include <boost/asio/any_completion_handler.hpp>
#include <boost/asio/associated_allocator.hpp>
#include <boost/asio/associated_cancellation_slot.hpp>
#include <boost/asio/associated_executor.hpp>
#include <boost/asio/async_result.hpp>
#include <boost/asio/deferred.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/experimental/parallel_group.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/prepend.hpp>

#include <array>
#include <chrono>

namespace boost::mqtt5::detail {

template <typename>
constexpr bool is_basic_socket = false;

template <typename P, typename E>
constexpr bool is_basic_socket<asio::basic_stream_socket<P, E>> = true;

namespace asio = boost::asio;

template <typename Owner>
class shutdown_op {
    struct on_locked {};
    struct on_shutdown {};

    Owner& _owner;

    using handler_type = asio::any_completion_handler<void (error_code)>;
    handler_type _handler;

public:
    template <typename Handler>
    shutdown_op(Owner& owner, Handler&& handler) :
        _owner(owner), _handler(std::move(handler))
    {}

    shutdown_op(shutdown_op&&) = default;
    shutdown_op(const shutdown_op&) = delete;

    shutdown_op& operator=(shutdown_op&&) = default;
    shutdown_op& operator=(const shutdown_op&) = delete;

    using allocator_type = asio::associated_allocator_t<handler_type>;
    allocator_type get_allocator() const noexcept {
        return asio::get_associated_allocator(_handler);
    }

    using cancellation_slot_type =
        asio::associated_cancellation_slot_t<handler_type>;
    cancellation_slot_type get_cancellation_slot() const noexcept {
        return asio::get_associated_cancellation_slot(_handler);
    }

    using executor_type = typename Owner::executor_type;
    executor_type get_executor() const noexcept {
        return _owner.get_executor();
    }

    void perform() {
        if constexpr (is_basic_socket<typename Owner::stream_type>) {
            error_code ec;
            _owner._stream_ptr->shutdown(asio::socket_base::shutdown_both, ec);
            return std::move(_handler)(error_code {});
        }
        else {
            if (_owner._conn_mtx.is_locked())
                return std::move(_handler)(error_code{});

            auto s = std::move(_owner._stream_ptr);
            _owner.replace_next_layer(_owner.construct_next_layer());
            _owner.open();

            _owner._conn_mtx.lock(
                asio::prepend(std::move(*this), on_locked {}, std::move(s))
            );
        }
    }

    void operator()(on_locked, typename Owner::stream_ptr s, error_code ec) {
        if (ec == asio::error::operation_aborted)
            return complete(s, asio::error::operation_aborted);

        if (!_owner.is_open()) {
            _owner._conn_mtx.unlock();
            return complete(s, asio::error::operation_aborted);
        }
        
        namespace asioex = boost::asio::experimental;

        // wait max 5 seconds for the shutdown op to finish
        _owner._connect_timer.expires_after(std::chrono::seconds(5));

        auto init_shutdown = [](
            auto handler, typename Owner::stream_type& stream
        ) {
            async_shutdown(stream, std::move(handler));
        };

        auto timed_shutdown = asioex::make_parallel_group(
            asio::async_initiate<const asio::deferred_t, void(error_code)>(
                init_shutdown, asio::deferred, std::ref(*s)
            ),
            _owner._connect_timer.async_wait(asio::deferred)
        );

        timed_shutdown.async_wait(
            asioex::wait_for_one(),
            asio::prepend(
                std::move(*this), on_shutdown {},
                std::move(s)
            )
        );
    }

    void operator()(
        on_shutdown, typename Owner::stream_ptr sptr,
        std::array<std::size_t, 2> /* ord */,
        error_code /* shutdown_ec */, error_code /* timer_ec */
    ) {
        _owner._conn_mtx.unlock();

        if (!_owner.is_open())
            return complete(sptr, asio::error::operation_aborted);

        // ignore shutdown error_code
        complete(sptr, error_code {});
    }

private:
    void complete(const typename Owner::stream_ptr& sptr, error_code ec) {
        asio::get_associated_cancellation_slot(_handler).clear();
        error_code close_ec;
        lowest_layer(*sptr).close(close_ec);
        std::move(_handler)(ec);
    }
};


} // end namespace boost::mqtt5::detail

#endif // !BOOST_MQTT5_SHUTDOWN_OP_HPP
