//
// Copyright (c) 2023-2026 Ivica Siladic, Bruno Iljazovic, Korina Simicevic
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MQTT5_PING_OP_HPP
#define BOOST_MQTT5_PING_OP_HPP

#include <boost/mqtt5/detail/control_packet.hpp>
#include <boost/mqtt5/detail/internal_types.hpp>

#include <boost/mqtt5/impl/codecs/message_encoders.hpp>

#include <boost/asio/consign.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/prepend.hpp>

#include <chrono>
#include <limits>

namespace boost::mqtt5::detail {

namespace asio = boost::asio;

template <typename ClientService, typename Handler>
class ping_op {
    using client_service = ClientService;
    using handler_type = Handler;

    struct on_timer {};
    struct on_pingreq {};

    std::shared_ptr<client_service> _svc_ptr;
    handler_type _handler;

public:
    ping_op(std::shared_ptr<client_service> svc_ptr, Handler&& handler) :
        _svc_ptr(std::move(svc_ptr)), _handler(std::move(handler))
    {}

    ping_op(ping_op&&) noexcept = default;
    ping_op(const ping_op&) = delete;

    ping_op& operator=(ping_op&&) noexcept = default;
    ping_op& operator=(const ping_op&) = delete;

    using allocator_type = asio::associated_allocator_t<handler_type>;
    allocator_type get_allocator() const noexcept {
        return asio::get_associated_allocator(_handler);
    }

    using executor_type = typename client_service::executor_type;
    executor_type get_executor() const noexcept {
        return _svc_ptr->get_executor();
    }

    void perform() {
        _svc_ptr->_ping_timer.expires_after(compute_wait_time());
        _svc_ptr->_ping_timer.async_wait(
            asio::prepend(std::move(*this), on_timer {})
        );
    }

    void operator()(on_timer, error_code ec) {
        if (!_svc_ptr->is_open())
            return complete();
        else if (ec == asio::error::operation_aborted)
            return perform();

        auto pingreq = control_packet<allocator_type>::of(
            no_pid, get_allocator(), encoders::encode_pingreq
        );

        auto wire_data = pingreq.wire_data();
        _svc_ptr->async_send(
            wire_data,
            no_serial, send_flag::none,
            asio::consign(
                asio::prepend(std::move(*this), on_pingreq {}),
                std::move(pingreq)
            )
        );
    }

    void operator()(on_pingreq, error_code ec) {
        if (!ec || ec == asio::error::try_again)
            return perform();

        complete();
    }

private:
    duration compute_wait_time() const {
        auto negotiated_ka = _svc_ptr->negotiated_keep_alive();
        return negotiated_ka ?
            std::chrono::seconds(negotiated_ka) :
            (duration::max)();
    }

    void complete() {
        return std::move(_handler)();
    }
};


} // end namespace boost::mqtt5::detail

#endif // !BOOST_MQTT5_PING_OP_HPP
