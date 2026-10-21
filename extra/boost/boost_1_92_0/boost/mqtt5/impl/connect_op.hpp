//
// Copyright (c) 2023-2026 Ivica Siladic, Bruno Iljazovic, Korina Simicevic
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MQTT5_CONNECT_OP_HPP
#define BOOST_MQTT5_CONNECT_OP_HPP

#include <boost/mqtt5/error.hpp>
#include <boost/mqtt5/reason_codes.hpp>

#include <boost/mqtt5/detail/async_traits.hpp>
#include <boost/mqtt5/detail/control_packet.hpp>
#include <boost/mqtt5/detail/internal_types.hpp>
#include <boost/mqtt5/detail/log_invoke.hpp>
#include <boost/mqtt5/detail/shutdown.hpp>

#include <boost/mqtt5/impl/codecs/message_decoders.hpp>
#include <boost/mqtt5/impl/codecs/message_encoders.hpp>

#include <boost/asio/any_completion_handler.hpp>
#include <boost/asio/append.hpp>
#include <boost/asio/associated_allocator.hpp>
#include <boost/asio/associated_cancellation_slot.hpp>
#include <boost/asio/associated_executor.hpp>
#include <boost/asio/cancellation_state.hpp>
#include <boost/asio/completion_condition.hpp>
#include <boost/asio/consign.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/prepend.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/write.hpp>

#include <cstdint>
#include <memory>
#include <string>

namespace boost::mqtt5::detail {

template <typename Stream, typename LoggerType>
class connect_op {
    static constexpr size_t min_packet_sz = 5;

    struct on_connect {};
    struct on_tls_handshake {};
    struct on_ws_handshake {};
    struct on_send_connect {};
    struct on_fixed_header {};
    struct on_read_packet {};
    struct on_init_auth_data {};
    struct on_auth_data {};
    struct on_send_auth {};
    struct on_complete_auth {};
    struct on_shutdown {};

    Stream& _stream;
    mqtt_ctx& _ctx;
    log_invoke<LoggerType>& _log;

    using handler_type = asio::any_completion_handler<void (error_code)>;
    handler_type _handler;

    std::unique_ptr<std::string> _buffer_ptr;
    asio::cancellation_state _cancellation_state;

    using endpoint = asio::ip::tcp::endpoint;

public:
    template <typename Handler>
    connect_op(
        Stream& stream, mqtt_ctx& ctx,
        log_invoke<LoggerType>& log,
        Handler&& handler
    ) :
        _stream(stream), _ctx(ctx), _log(log),
        _handler(std::forward<Handler>(handler)),
        _cancellation_state(
            asio::get_associated_cancellation_slot(_handler),
            asio::enable_total_cancellation {},
            asio::enable_total_cancellation {}
        )
    {}

    connect_op(connect_op&&) = default;
    connect_op(const connect_op&) = delete;

    connect_op& operator=(connect_op&&) = default;
    connect_op& operator=(const connect_op&) = delete;

    using allocator_type = asio::associated_allocator_t<handler_type>;
    allocator_type get_allocator() const noexcept {
        return asio::get_associated_allocator(_handler);
    }

    using cancellation_slot_type = asio::cancellation_slot;
    cancellation_slot_type get_cancellation_slot() const noexcept {
        return _cancellation_state.slot();
    }

    using executor_type = asio::associated_executor_t<handler_type>;
    executor_type get_executor() const noexcept {
        return asio::get_associated_executor(_handler);
    }

    void perform(const endpoint& ep, authority_path ap) {
        lowest_layer(_stream).async_connect(
            ep,
            asio::append(
                asio::prepend(std::move(*this), on_connect {}),
                ep, std::move(ap)
            )
        );
    }

    void operator()(
        on_connect, error_code ec, endpoint ep, authority_path ap
    ) {
        if (is_cancelled())
            return complete(asio::error::operation_aborted);

        _log.at_tcp_connect(ec, ep);
        if (ec)
            return complete(ec);

        do_tls_handshake(std::move(ep), std::move(ap));
    }

    void do_tls_handshake(endpoint ep, authority_path ap) {
        if constexpr (has_tls_handshake<Stream>) {
            _stream.async_handshake(
                tls_handshake_type<Stream>::client,
                asio::append(
                    asio::prepend(std::move(*this), on_tls_handshake {}),
                    std::move(ep), std::move(ap)
                )
            );
        }
        else if constexpr (
            has_tls_handshake<next_layer_type<Stream>>
        ) {
            _stream.next_layer().async_handshake(
                tls_handshake_type<next_layer_type<Stream>>::client,
                asio::append(
                    asio::prepend(std::move(*this), on_tls_handshake {}),
                    std::move(ep), std::move(ap)
                )
            );
        }
        else
            do_ws_handshake(std::move(ep), std::move(ap));
    }

    void operator()(
        on_tls_handshake, error_code ec, endpoint ep, authority_path ap
    ) {
        if (is_cancelled())
            return complete(asio::error::operation_aborted);

        _log.at_tls_handshake(ec, ep);
        if (ec)
            return complete(ec);

        do_ws_handshake(std::move(ep), std::move(ap));
    }

    void do_ws_handshake(endpoint ep, authority_path ap) {
        if constexpr (has_ws_handshake<Stream>)
            // If you get a compilation error here,
            // it might be because of a missing <boost/mqtt5/websocket.hpp> include
            ws_handshake_traits<Stream>::async_handshake(
                _stream, std::move(ap),
                asio::append(
                    asio::prepend(std::move(*this), on_ws_handshake {}), ep
                )
            );
        else
            (*this)(on_ws_handshake {}, error_code {}, ep);
    }

    void operator()(on_ws_handshake, error_code ec, endpoint ep) {
        if (is_cancelled())
            return complete(asio::error::operation_aborted);

        if constexpr (has_ws_handshake<Stream>)
            _log.at_ws_handshake(ec, ep);

        if (ec)
            return complete(ec);

        auto auth_method = _ctx.authenticator.method();
        if (!auth_method.empty()) {
            _ctx.co_props[prop::authentication_method] = auth_method;
            return _ctx.authenticator.async_auth(
                auth_step_e::client_initial, "",
                asio::prepend(std::move(*this), on_init_auth_data {})
            );
        }

        send_connect();
    }

    void operator()(on_init_auth_data, error_code ec, std::string data) {
        if (is_cancelled())
            return complete(asio::error::operation_aborted);

        if (ec)
            return do_shutdown(asio::error::try_again);

        _ctx.co_props[prop::authentication_data] = std::move(data);
        send_connect();
    }

    void send_connect() {
        if (!_ctx.co_props[prop::maximum_packet_size].has_value())
            _ctx.co_props[prop::maximum_packet_size] = default_max_recv_size;
        auto packet = control_packet<allocator_type>::of(
            no_pid, get_allocator(),
            encoders::encode_connect,
            _ctx.creds.client_id,
            _ctx.creds.username, _ctx.creds.password,
            _ctx.keep_alive, false, _ctx.co_props, _ctx.will_msg
        );

        auto wire_data = packet.wire_data();

        detail::async_write(
            _stream, asio::buffer(wire_data),
            asio::consign(
                asio::prepend(std::move(*this), on_send_connect {}),
                std::move(packet)
            )
        );
    }

    void operator()(on_send_connect, error_code ec, size_t) {
        if (is_cancelled())
            return complete(asio::error::operation_aborted);

        if (ec) {
            _log.at_transport_error(ec);
            return do_shutdown(ec);
        }

        _buffer_ptr = std::make_unique<std::string>(min_packet_sz, char(0));

        auto buff = asio::buffer(_buffer_ptr->data(), min_packet_sz);
        asio::async_read(
            _stream, buff, asio::transfer_all(),
            asio::prepend(std::move(*this), on_fixed_header {})
        );
    }

    void operator()(
        on_fixed_header, error_code ec, size_t num_read
    ) {
        if (is_cancelled())
            return complete(asio::error::operation_aborted);

        if (ec) {
            _log.at_transport_error(ec);
            return do_shutdown(ec);
        }

        auto code = control_code_e((*_buffer_ptr)[0] & 0b11110000);

        if (code != control_code_e::auth && code != control_code_e::connack)
            return do_shutdown(asio::error::try_again);

        auto varlen_ptr = _buffer_ptr->cbegin() + 1;
        auto varlen = decoders::type_parse(
            varlen_ptr, _buffer_ptr->cend(), decoders::basic::varint_
        );

        if (!varlen)
            return do_shutdown(asio::error::try_again);

        auto varlen_sz = std::distance(_buffer_ptr->cbegin() + 1, varlen_ptr);
        auto body_already_read =
            std::distance(varlen_ptr, _buffer_ptr->cbegin() + num_read);

        if (*varlen < body_already_read)
            return do_shutdown(client::error::malformed_packet);

        size_t remain_len = static_cast<size_t>(*varlen - body_already_read);
        size_t total_len = num_read + remain_len;

        auto max_packet_size = _ctx.co_props[prop::maximum_packet_size]
            .value_or(default_max_recv_size);
        if (total_len > max_packet_size)
            return do_shutdown(client::error::malformed_packet);

        if (total_len > _buffer_ptr->size())
            _buffer_ptr->resize(total_len);

        auto buff = asio::buffer(_buffer_ptr->data() + num_read, remain_len);
        auto first = _buffer_ptr->cbegin() + varlen_sz + 1;
        auto last = first + *varlen;

        asio::async_read(
            _stream, buff, asio::transfer_all(),
            asio::prepend(
                asio::append(std::move(*this), code, first, last),
                on_read_packet {}
            )
        );
    }

    void operator()(
        on_read_packet, error_code ec, size_t, control_code_e code,
        byte_citer first, byte_citer last
    ) {
        if (is_cancelled())
            return complete(asio::error::operation_aborted);

        if (ec) {
            _log.at_transport_error(ec);
            return do_shutdown(ec);
        }

        if (code == control_code_e::connack)
            return on_connack(first, last);

        if (!_ctx.co_props[prop::authentication_method].has_value())
            return do_shutdown(client::error::malformed_packet);

        on_auth(first, last);
    }

    void on_connack(byte_citer first, byte_citer last) {
        auto packet_length = static_cast<uint32_t>(std::distance(first, last));
        auto rv = decoders::decode_connack(packet_length, first);
        if (!rv.has_value())
            return do_shutdown(client::error::malformed_packet);
        const auto& [session_present, reason_code, ca_props] = *rv;

        _ctx.ca_props = ca_props;
        _ctx.state.session_present(session_present);

        //  Unexpected result handling:
        //  - If we don't have a Session State, and we get session_present = true,
        //      we must close the network connection (and restart with a clean start)
        //  - If we have a Session State, and we get session_present = false,
        //      we must discard our Session State

        auto rc = to_reason_code<reason_codes::category::connack>(reason_code);
        if (!rc.has_value()) // reason code not allowed in CONNACK
            return do_shutdown(client::error::malformed_packet);

        _log.at_connack(*rc, session_present, ca_props);
        if (*rc)
            return do_shutdown(asio::error::try_again);

        if (_ctx.co_props[prop::authentication_method].has_value())
            return _ctx.authenticator.async_auth(
                auth_step_e::server_final,
                ca_props[prop::authentication_data].value_or(""),
                asio::prepend(std::move(*this), on_complete_auth {})
            );

        complete(error_code {});
    }

    void on_auth(byte_citer first, byte_citer last) {
        auto packet_length = static_cast<uint32_t>(std::distance(first, last));
        auto rv = decoders::decode_auth(packet_length, first);
        if (!rv.has_value())
            return do_shutdown(client::error::malformed_packet);
        const auto& [reason_code, auth_props] = *rv;

        auto rc = to_reason_code<reason_codes::category::auth>(reason_code);
        if (
            !rc.has_value() ||
            auth_props[prop::authentication_method]
                != _ctx.co_props[prop::authentication_method]
        )
            return do_shutdown(client::error::malformed_packet);

        _ctx.authenticator.async_auth(
            auth_step_e::server_challenge,
            auth_props[prop::authentication_data].value_or(""),
            asio::prepend(std::move(*this), on_auth_data {})
        );
    }

    void operator()(on_auth_data, error_code ec, std::string data) {
        if (is_cancelled())
            return complete(asio::error::operation_aborted);

        if (ec)
            return do_shutdown(asio::error::try_again);

        auth_props props;
        props[prop::authentication_method] =
            _ctx.co_props[prop::authentication_method];
        props[prop::authentication_data] = std::move(data);

        auto packet = control_packet<allocator_type>::of(
            no_pid, get_allocator(),
            encoders::encode_auth,
            reason_codes::continue_authentication.value(), props
        );

        auto wire_data = packet.wire_data();

        detail::async_write(
            _stream, asio::buffer(wire_data),
            asio::consign(
                asio::prepend(std::move(*this), on_send_auth {}),
                std::move(packet)
            )
        );
    }

    void operator()(on_send_auth, error_code ec, size_t) {
        if (is_cancelled())
            return complete(asio::error::operation_aborted);

        if (ec) {
            _log.at_transport_error(ec);
            return do_shutdown(ec);
        }

        auto buff = asio::buffer(_buffer_ptr->data(), min_packet_sz);
        asio::async_read(
            _stream, buff, asio::transfer_all(),
            asio::prepend(std::move(*this), on_fixed_header {})
        );
    }

    void operator()(on_complete_auth, error_code ec, std::string) {
        if (is_cancelled())
            return complete(asio::error::operation_aborted);

        if (ec)
            return do_shutdown(asio::error::try_again);

        complete(error_code {});
    }

    void do_shutdown(error_code connect_ec) {
        auto init_shutdown = [&stream = _stream](auto handler) {
            async_shutdown(stream, std::move(handler));
        };
        auto token = asio::prepend(std::move(*this), on_shutdown{}, connect_ec);

        return asio::async_initiate<decltype(token), void(error_code)>(
            init_shutdown, token
        );
    }

    void operator()(on_shutdown, error_code connect_ec, error_code) {
        // ignore shutdown error_code
        complete(connect_ec);
    }

private:
    bool is_cancelled() const {
        return _cancellation_state.cancelled() != asio::cancellation_type::none;
    }

    void complete(error_code ec) {
        asio::get_associated_cancellation_slot(_handler).clear();
        std::move(_handler)(ec);
    }
};


} // end namespace boost::mqtt5::detail

#endif // !BOOST_MQTT5_CONNECT_OP_HPP
