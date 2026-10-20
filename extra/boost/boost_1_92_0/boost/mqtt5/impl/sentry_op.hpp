//
// Copyright (c) 2023-2026 Ivica Siladic, Bruno Iljazovic, Korina Simicevic
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MQTT5_SENTRY_OP_HPP
#define BOOST_MQTT5_SENTRY_OP_HPP

#include <boost/mqtt5/error.hpp>
#include <boost/mqtt5/types.hpp>

#include <boost/mqtt5/impl/disconnect_op.hpp>

#include <boost/asio/prepend.hpp>

#include <chrono>
#include <memory>

namespace boost::mqtt5::detail {

namespace asio = boost::asio;

template <typename ClientService, typename Handler>
class sentry_op {
    using client_service = ClientService;
    using handler_type = Handler;

    struct on_timer {};
    struct on_disconnect {};

    static constexpr auto check_interval = std::chrono::seconds(3);

    std::shared_ptr<client_service> _svc_ptr;
    handler_type _handler;

public:
    sentry_op(std::shared_ptr<client_service> svc_ptr, Handler&& handler) :
        _svc_ptr(std::move(svc_ptr)), _handler(std::move(handler))
    {}

    sentry_op(sentry_op&&) noexcept = default;
    sentry_op(const sentry_op&) = delete;

    sentry_op& operator=(sentry_op&&) noexcept = default;
    sentry_op& operator=(const sentry_op&) = delete;

    using allocator_type = asio::associated_allocator_t<handler_type>;
    allocator_type get_allocator() const noexcept {
        return asio::get_associated_allocator(_handler);
    }

    using executor_type = typename client_service::executor_type;
    executor_type get_executor() const noexcept {
        return _svc_ptr->get_executor();
    }

    void perform() {
        _svc_ptr->_sentry_timer.expires_after(check_interval);
        _svc_ptr->_sentry_timer.async_wait(
            asio::prepend(std::move(*this), on_timer {})
        );
    }

    void operator()(on_timer, error_code) {
        if (!_svc_ptr->is_open())
            return complete();

        if (_svc_ptr->_replies.any_expired()) {
            auto props = disconnect_props {};
            // TODO add what packet was expected?
            props[prop::reason_string] = "No reply received within 20 seconds";
            auto svc_ptr = _svc_ptr;
            return async_disconnect(
                disconnect_rc_e::unspecified_error, props, svc_ptr,
                asio::prepend(std::move(*this), on_disconnect {})
            );
        }

        perform();
    }

    void operator()(on_disconnect, error_code ec) {
        if (ec)
            return complete();
        
        perform();
    }

private:
    void complete() {
        return std::move(_handler)();
    }
};


} // end namespace boost::mqtt5::detail

#endif // !BOOST_MQTT5_SENTRY_OP_HPP
