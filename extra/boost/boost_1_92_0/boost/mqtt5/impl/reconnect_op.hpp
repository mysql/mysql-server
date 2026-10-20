//
// Copyright (c) 2023-2026 Ivica Siladic, Bruno Iljazovic, Korina Simicevic
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MQTT5_RECONNECT_OP_HPP
#define BOOST_MQTT5_RECONNECT_OP_HPP

#include <boost/mqtt5/types.hpp>

#include <boost/mqtt5/detail/async_traits.hpp>
#include <boost/mqtt5/detail/internal_types.hpp>

#include <boost/mqtt5/impl/connect_op.hpp>

#include <boost/asio/any_completion_handler.hpp>
#include <boost/asio/associated_allocator.hpp>
#include <boost/asio/associated_cancellation_slot.hpp>
#include <boost/asio/associated_executor.hpp>
#include <boost/asio/async_result.hpp>
#include <boost/asio/deferred.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/experimental/parallel_group.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/prepend.hpp>
#include <boost/random/linear_congruential.hpp>
#include <boost/random/uniform_smallint.hpp>

#include <array>
#include <chrono>
#include <memory>
#include <string>

namespace boost::mqtt5::detail {

class exponential_backoff {
    int _curr_exp { 0 };

    static constexpr int _base_mulptilier = 1000;
    static constexpr int _max_exp = 4;

    // sizeof(_generator) = 8
    boost::random::rand48 _generator { uint32_t(std::time(0)) };
    boost::random::uniform_smallint<> _distribution { -500, 500 };
public:
    exponential_backoff() = default;

    duration generate() {
        int exponent = _curr_exp < _max_exp ? _curr_exp++ : _max_exp;
        int base = 1 << exponent;
        return std::chrono::milliseconds(
            base * _base_mulptilier + _distribution(_generator) /* noise */
        );
    }
};

namespace asio = boost::asio;

template <typename Owner>
class reconnect_op {
    struct on_locked {};
    struct on_next_endpoint {};
    struct on_connect {};
    struct on_backoff {};

    Owner& _owner;

    using handler_type = asio::any_completion_handler<void (error_code)>;
    handler_type _handler;

    std::unique_ptr<std::string> _buffer_ptr;

    exponential_backoff _generator;

    using endpoint = asio::ip::tcp::endpoint;
    using epoints = asio::ip::tcp::resolver::results_type;

public:
    template <typename Handler>
    reconnect_op(Owner& owner, Handler&& handler) :
        _owner(owner), _handler(std::move(handler))
    {}

    reconnect_op(reconnect_op&&) = default;
    reconnect_op(const reconnect_op&) = delete;

    reconnect_op& operator=(reconnect_op&&) = default;
    reconnect_op& operator=(const reconnect_op&) = delete;

    using allocator_type = asio::associated_allocator_t<handler_type>;
    allocator_type get_allocator() const noexcept {
        return asio::get_associated_allocator(_handler);
    }

    using cancellation_slot_type =
        asio::associated_cancellation_slot_t<handler_type>;
    cancellation_slot_type get_cancellation_slot() const noexcept {
        return asio::get_associated_cancellation_slot(_handler);
    }

    using executor_type = asio::associated_executor_t<handler_type>;
    executor_type get_executor() const noexcept {
        return asio::get_associated_executor(_handler);
    }

    void perform(typename Owner::stream_ptr s) {
        _owner._conn_mtx.lock(
            asio::prepend(std::move(*this), on_locked {}, s)
        );
    }

    void operator()(on_locked, typename Owner::stream_ptr s, error_code ec) {
        if (ec == asio::error::operation_aborted)
            // cancelled without acquiring the lock (by calling client.cancel())
            return std::move(_handler)(ec);
        
        if (!_owner.is_open())
            return complete(asio::error::operation_aborted);

        if (s != _owner._stream_ptr)
            return complete(asio::error::try_again);

        do_reconnect();
    }

    void do_reconnect() {
        _owner._endpoints.async_next_endpoint(
            asio::prepend(std::move(*this), on_next_endpoint {})
        );
    }

    void backoff_and_reconnect() {
        _owner._connect_timer.expires_after(_generator.generate());
        _owner._connect_timer.async_wait(
            asio::prepend(std::move(*this), on_backoff {})
        );
    }

    void operator()(on_backoff, error_code ec) {
        if (ec == asio::error::operation_aborted || !_owner.is_open())
            return complete(asio::error::operation_aborted);

        do_reconnect();
    }

    void operator()(
        on_next_endpoint, error_code ec,
        epoints eps, authority_path ap
    ) {
        // the three error codes below are the only possible codes
        // that may be returned from async_next_endpont

        if (ec == asio::error::operation_aborted || !_owner.is_open())
            return complete(asio::error::operation_aborted);

        if (ec == asio::error::try_again)
            return backoff_and_reconnect();

        if (ec == asio::error::host_not_found)
            return complete(asio::error::no_recovery);

        connect(eps.cbegin(), std::move(ap));
    }

    void connect(epoints::const_iterator eps, authority_path ap) {
        namespace asioex = boost::asio::experimental;

        const auto& ep = eps->endpoint();
        auto sptr = _owner.construct_and_open_next_layer(ep.protocol());

        if constexpr (has_tls_context<typename Owner::stream_context_type>)
            setup_tls_sni(
                ap, _owner._stream_context.tls_context(), *sptr
            );

        // wait max 5 seconds for the connect (handshake) op to finish
        _owner._connect_timer.expires_after(std::chrono::seconds(5));

        auto init_connect = [](
            auto handler, typename Owner::stream_type& stream,
            mqtt_ctx& context, log_invoke<typename Owner::logger_type>& log,
            endpoint ep, authority_path ap
        ) {
            connect_op { stream, context, log, std::move(handler) }
                .perform(ep, std::move(ap));
        };

        auto timed_connect = asioex::make_parallel_group(
            asio::async_initiate<const asio::deferred_t, void(error_code)>(
                init_connect, asio::deferred, std::ref(*sptr),
                std::ref(_owner._stream_context.mqtt_context()),
                std::ref(_owner.log()),
                ep, ap
            ),
            _owner._connect_timer.async_wait(asio::deferred)
        );

        timed_connect.async_wait(
            asioex::wait_for_one(),
            asio::prepend(
                std::move(*this), on_connect {},
                std::move(sptr), std::move(eps), std::move(ap)
            )
        );
    }

    void operator()(
        on_connect,
        typename Owner::stream_ptr sptr, epoints::const_iterator eps, authority_path ap,
        std::array<std::size_t, 2> ord,
        error_code connect_ec, error_code timer_ec
    ) {
        // connect_ec may be any of:
        //  1) async_connect error codes
        //  2) async_handshake (TLS) error codes
        //  3) async_handshake (WebSocket) error codes
        //  4) async_write error codes
        //  5) async_read error codes
        //  5) client::error::malformed_packet
        if (
            (ord[0] == 0 && connect_ec == asio::error::operation_aborted) ||
            (ord[0] == 1 && timer_ec == asio::error::operation_aborted) ||
            !_owner.is_open()
        )
            return complete(asio::error::operation_aborted);

        // retry for operation timed out and any other error_code or client::error::malformed_packet
        if (ord[0] == 1 || connect_ec) {
            // if the hostname resolved into more endpoints, try the next one
            if (++eps != epoints::const_iterator())
                return connect(std::move(eps), std::move(ap));
            // try next server
            return do_reconnect();
        }

        _owner.replace_next_layer(std::move(sptr));
        complete(error_code {});
    }

private:
    void complete(error_code ec) {
        _owner._conn_mtx.unlock();
        std::move(_handler)(ec);
    }
};


} // end namespace boost::mqtt5::detail

#endif // !BOOST_MQTT5_RECONNECT_OP_HPP
