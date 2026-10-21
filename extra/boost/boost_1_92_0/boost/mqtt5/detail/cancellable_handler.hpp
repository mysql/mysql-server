//
// Copyright (c) 2023-2026 Ivica Siladic, Bruno Iljazovic, Korina Simicevic
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MQTT5_CANCELLABLE_HANDLER_HPP
#define BOOST_MQTT5_CANCELLABLE_HANDLER_HPP

#include <boost/mqtt5/detail/async_traits.hpp>

#include <boost/asio/associated_allocator.hpp>
#include <boost/asio/associated_cancellation_slot.hpp>
#include <boost/asio/associated_executor.hpp>
#include <boost/asio/associated_immediate_executor.hpp>
#include <boost/asio/cancellation_state.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/prepend.hpp>

namespace boost::mqtt5::detail {

template <typename Handler, typename Executor>
class cancellable_handler {
    Executor _executor;
    Handler _handler;
    tracking_type<Handler, Executor> _handler_ex;
    asio::cancellation_state _cancellation_state;

public:
    cancellable_handler(Handler&& handler, const Executor& ex) :
        _executor(ex),
        _handler(std::move(handler)),
        _handler_ex(boost::mqtt5::detail::tracking_executor(_handler, ex)),
        _cancellation_state(
            asio::get_associated_cancellation_slot(_handler),
            asio::enable_total_cancellation {},
            asio::enable_terminal_cancellation {}
        )
    {}

    cancellable_handler(cancellable_handler&&) = default;
    cancellable_handler(const cancellable_handler&) = delete;

    cancellable_handler& operator=(cancellable_handler&&) = default;
    cancellable_handler& operator=(const cancellable_handler&) = delete;

    using allocator_type = asio::associated_allocator_t<Handler>;
    allocator_type get_allocator() const noexcept {
        return asio::get_associated_allocator(_handler);
    }

    using cancellation_slot_type = asio::associated_cancellation_slot_t<Handler>;
    cancellation_slot_type get_cancellation_slot() const noexcept {
        return _cancellation_state.slot();
    }

    using executor_type = tracking_type<Handler, Executor>;
    executor_type get_executor() const noexcept {
        return _handler_ex;
    }
    
    using immediate_executor_type =
        asio::associated_immediate_executor_t<Handler, Executor>;
    immediate_executor_type get_immediate_executor() const noexcept {
        // get_associated_immediate_executor will require asio::execution::blocking.never
        // on the default executor.
        return asio::get_associated_immediate_executor(_handler, _executor);
    }

    asio::cancellation_type_t cancelled() const {
        return _cancellation_state.cancelled();
    }

    template <typename... Args>
    void complete(Args&&... args) {
        asio::get_associated_cancellation_slot(_handler).clear();
        asio::dispatch(
            _handler_ex,
            asio::prepend(std::move(_handler), std::forward<Args>(args)...)
        );
    }

    template <typename... Args>
    void complete_immediate(Args&&... args) {
        asio::get_associated_cancellation_slot(_handler).clear();
        auto ex = get_immediate_executor();
        asio::dispatch(
            ex,
            asio::prepend(std::move(_handler), std::forward<Args>(args)...)
        );
    }

};

} // end boost::mqtt5::detail

#endif // !BOOST_MQTT5_CANCELLABLE_HANDLER_HPP
