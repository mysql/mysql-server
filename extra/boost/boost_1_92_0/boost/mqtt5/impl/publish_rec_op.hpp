//
// Copyright (c) 2023-2026 Ivica Siladic, Bruno Iljazovic, Korina Simicevic
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MQTT5_PUBLISH_REC_OP_HPP
#define BOOST_MQTT5_PUBLISH_REC_OP_HPP

#include <boost/mqtt5/error.hpp>
#include <boost/mqtt5/property_types.hpp>
#include <boost/mqtt5/reason_codes.hpp>
#include <boost/mqtt5/types.hpp>

#include <boost/mqtt5/detail/control_packet.hpp>
#include <boost/mqtt5/detail/internal_types.hpp>

#include <boost/mqtt5/impl/codecs/message_decoders.hpp>
#include <boost/mqtt5/impl/codecs/message_encoders.hpp>
#include <boost/mqtt5/impl/disconnect_op.hpp>

#include <boost/asio/consign.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/prepend.hpp>
#include <boost/asio/recycling_allocator.hpp>

#include <cstdint>
#include <memory>
#include <string>

namespace boost::mqtt5::detail {

namespace asio = boost::asio;

template <typename ClientService>
class publish_rec_op {
    using client_service = ClientService;

    struct on_puback {};
    struct on_pubrec {};
    struct on_pubrel {};
    struct on_pubcomp {};

    std::shared_ptr<client_service> _svc_ptr;
    decoders::publish_message _message;

public:
    explicit publish_rec_op(std::shared_ptr<client_service> svc_ptr) :
        _svc_ptr(std::move(svc_ptr))
    {}

    publish_rec_op(publish_rec_op&&) noexcept = default;
    publish_rec_op(const publish_rec_op&) = delete;

    publish_rec_op& operator=(publish_rec_op&&) noexcept = default;
    publish_rec_op& operator=(const publish_rec_op&) = delete;

    using allocator_type = asio::recycling_allocator<void>;
    allocator_type get_allocator() const noexcept {
        return allocator_type {};
    }

    using executor_type = typename client_service::executor_type;
    executor_type get_executor() const noexcept {
        return _svc_ptr->get_executor();
    }

    void perform(decoders::publish_message message) {
        auto flags = std::get<2>(message);
        auto qos_bits = (flags >> 1) & 0b11;
        if (qos_bits == 0b11)
            return on_malformed_packet(
                "Malformed PUBLISH received: QoS bits set to 0b11"
            );

        auto qos = qos_e(qos_bits);
        _message = std::move(message);

        if (qos == qos_e::at_most_once)
            return complete();

        auto packet_id = std::get<1>(_message);

        if (qos == qos_e::at_least_once) {
            auto puback = control_packet<allocator_type>::of(
                with_pid, get_allocator(),
                encoders::encode_puback, *packet_id,
                uint8_t(0), puback_props {}
            );
            return send_puback(std::move(puback));
        }

        // qos == qos_e::exactly_once
        auto pubrec = control_packet<allocator_type>::of(
            with_pid, get_allocator(),
            encoders::encode_pubrec, *packet_id,
            uint8_t(0), pubrec_props {}
        );

        return send_pubrec(std::move(pubrec));
    }

    void send_puback(control_packet<allocator_type> puback) {
        auto wire_data = puback.wire_data();
        _svc_ptr->async_send(
            wire_data,
            no_serial, send_flag::none,
            asio::consign(
                asio::prepend(std::move(*this), on_puback {}),
                std::move(puback)
            )
        );
    }

    void operator()(on_puback, error_code ec) {
        if (ec)
            return;

        complete();
    }

    void send_pubrec(control_packet<allocator_type> pubrec) {
        auto wire_data = pubrec.wire_data();
        _svc_ptr->async_send(
            wire_data,
            no_serial, send_flag::none,
            asio::prepend(std::move(*this), on_pubrec {}, std::move(pubrec))
        );
    }

    void operator()(
        on_pubrec, control_packet<allocator_type> packet,
        error_code ec
    ) {
        if (ec)
            return;

        wait_pubrel(packet.packet_id());
    }

    void wait_pubrel(uint16_t packet_id) {
        _svc_ptr->async_wait_reply(
            control_code_e::pubrel, packet_id,
            asio::prepend(std::move(*this), on_pubrel {}, packet_id)
        );
    }

    void operator()(
        on_pubrel, uint16_t packet_id,
        error_code ec, byte_citer first, byte_citer last
    ) {
        if (ec == asio::error::try_again) // "resend unanswered"
            return wait_pubrel(packet_id);

        if (ec)
            return;

        auto pubrel = decoders::decode_pubrel(static_cast<uint32_t>(std::distance(first, last)), first);
        if (!pubrel.has_value()) {
            on_malformed_packet("Malformed PUBREL received: cannot decode");
            return wait_pubrel(packet_id);
        }

        auto& [reason_code, props] = *pubrel;
        auto rc = to_reason_code<reason_codes::category::pubrel>(reason_code);
        if (!rc) {
            on_malformed_packet("Malformed PUBREL received: invalid Reason Code");
            return wait_pubrel(packet_id);
        }

        auto pubcomp = control_packet<allocator_type>::of(
            with_pid, get_allocator(),
            encoders::encode_pubcomp, packet_id,
            uint8_t(0), pubcomp_props{}
        );
        send_pubcomp(std::move(pubcomp));
    }

    void send_pubcomp(control_packet<allocator_type> pubcomp) {
        auto wire_data = pubcomp.wire_data();
        _svc_ptr->async_send(
            wire_data,
            no_serial, send_flag::none,
            asio::prepend(std::move(*this), on_pubcomp {}, std::move(pubcomp))
        );
    }

    void operator()(
        on_pubcomp, control_packet<allocator_type> packet,
        error_code ec
    ) {
        if (ec == asio::error::try_again)
            return wait_pubrel(packet.packet_id());

        if (ec)
            return;

        complete();
    }

private:
    void on_malformed_packet(const std::string& reason) {
        auto props = disconnect_props {};
        props[prop::reason_string] = reason;
        return async_disconnect(
            disconnect_rc_e::malformed_packet, props,
            _svc_ptr, asio::detached
        );
    }

    void complete() {
        /* auto rv = */_svc_ptr->channel_store(std::move(_message));
    }
};

} // end namespace boost::mqtt5::detail

#endif // !BOOST_MQTT5_PUBLISH_REC_OP_HPP
