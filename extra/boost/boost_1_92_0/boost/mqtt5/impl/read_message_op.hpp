//
// Copyright (c) 2023-2026 Ivica Siladic, Bruno Iljazovic, Korina Simicevic
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MQTT5_READ_MESSAGE_OP_HPP
#define BOOST_MQTT5_READ_MESSAGE_OP_HPP

#include <boost/mqtt5/reason_codes.hpp>
#include <boost/mqtt5/types.hpp>

#include <boost/mqtt5/detail/control_packet.hpp>

#include <boost/mqtt5/impl/codecs/message_decoders.hpp>
#include <boost/mqtt5/impl/disconnect_op.hpp>
#include <boost/mqtt5/impl/publish_rec_op.hpp>
#include <boost/mqtt5/impl/re_auth_op.hpp>

#include <boost/asio/error.hpp>
#include <boost/asio/prepend.hpp>
#include <boost/asio/recycling_allocator.hpp>
#include <boost/assert.hpp>

#include <cstdint>
#include <memory>

namespace boost::mqtt5::detail {

namespace asio = boost::asio;

template <typename ClientService, typename Handler>
class read_message_op {
    using client_service = ClientService;
    using handler_type = Handler;

    struct on_message {};
    struct on_disconnect {};

    std::shared_ptr<client_service> _svc_ptr;
    handler_type _handler;

public:
    read_message_op(std::shared_ptr<client_service> svc_ptr, Handler&& handler)
        : _svc_ptr(std::move(svc_ptr)), _handler(std::move(handler))
    {}

    read_message_op(read_message_op&&) noexcept = default;
    read_message_op(const read_message_op&) = delete;

    read_message_op& operator=(read_message_op&&) noexcept = default;
    read_message_op& operator=(const read_message_op&) = delete;

    using allocator_type = asio::associated_allocator_t<handler_type>;
    allocator_type get_allocator() const noexcept {
        return asio::get_associated_allocator(_handler);
    }

    using executor_type = typename client_service::executor_type;
    executor_type get_executor() const noexcept {
        return _svc_ptr->get_executor();
    }

    void perform() {
        _svc_ptr->async_assemble(
            asio::prepend(std::move(*this), on_message {})
        );
    }

    void operator()(
        on_message, error_code ec,
        uint8_t control_code,
        byte_citer first, byte_citer last
    ) {
        if (ec == client::error::malformed_packet)
            return on_malformed_packet(
                disconnect_rc_e::malformed_packet,
                "Malformed Packet received from the Server"
            );
        else if (ec == client::error::packet_too_large)
            return on_malformed_packet(
                disconnect_rc_e::packet_too_large,
                "The packet size is greater than Maximum Packet Size"
            );

        if (ec == asio::error::no_recovery)
            _svc_ptr->cancel();

        if (ec)
            return complete();

        dispatch(control_code, first, last);
    }

    void operator()(on_disconnect, error_code ec) {
        if (ec)
            return complete();

        perform();
    }

private:
    void dispatch(
        uint8_t control_byte,
        byte_citer first, byte_citer last
    ) {
        auto code = control_code_e(control_byte & 0b11110000);

        switch (code) {
            case control_code_e::publish: {
                auto msg = decoders::decode_publish(
                    control_byte, static_cast<uint32_t>(std::distance(first, last)), first
                );
                if (!msg.has_value())
                    return on_malformed_packet(
                        disconnect_rc_e::malformed_packet,
                        "Malformed PUBLISH received: cannot decode"
                    );

                publish_rec_op { _svc_ptr }.perform(std::move(*msg));
            }
            break;
            case control_code_e::disconnect: {
                auto rv = decoders::decode_disconnect(
                    static_cast<uint32_t>(std::distance(first, last)), first
                );
                if (!rv.has_value())
                    return on_malformed_packet(
                        disconnect_rc_e::malformed_packet,
                        "Malformed DISCONNECT received: cannot decode"
                    );

                const auto& [rc, props] = *rv;
                _svc_ptr->log().at_disconnect(
                    to_reason_code<reason_codes::category::disconnect>(rc)
                        .value_or(reason_codes::unspecified_error),
                    props
                );
                return _svc_ptr->async_shutdown(
                    asio::prepend(std::move(*this), on_disconnect {})
                );
            }
            break;
            case control_code_e::auth: {
                auto rv = decoders::decode_auth(
                    static_cast<uint32_t>(std::distance(first, last)), first
                );
                if (!rv.has_value())
                    return on_malformed_packet(
                        disconnect_rc_e::malformed_packet,
                        "Malformed AUTH received: cannot decode"
                    );

                re_auth_op { _svc_ptr }.perform(std::move(*rv));
            }
            break;
            default:
                BOOST_ASSERT(false);
        }

        perform();
    }

    void on_malformed_packet(disconnect_rc_e rc, const std::string& reason) {
        auto props = disconnect_props {};
        props[prop::reason_string] = reason;
        auto svc_ptr = _svc_ptr; // copy before this is moved

        async_disconnect(
            rc, props, svc_ptr,
            asio::prepend(std::move(*this), on_disconnect {})
        );
    }

    void complete() {
        return std::move(_handler)();
    }
};


} // end namespace boost::mqtt5::detail

#endif // !BOOST_MQTT5_READ_MESSAGE_OP_HPP
