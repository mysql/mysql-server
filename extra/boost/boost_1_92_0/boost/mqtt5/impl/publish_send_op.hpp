//
// Copyright (c) 2023-2026 Ivica Siladic, Bruno Iljazovic, Korina Simicevic
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MQTT5_PUBLISH_SEND_OP_HPP
#define BOOST_MQTT5_PUBLISH_SEND_OP_HPP

#include <boost/mqtt5/error.hpp>
#include <boost/mqtt5/reason_codes.hpp>
#include <boost/mqtt5/types.hpp>

#include <boost/mqtt5/detail/cancellable_handler.hpp>
#include <boost/mqtt5/detail/control_packet.hpp>
#include <boost/mqtt5/detail/internal_types.hpp>
#include <boost/mqtt5/detail/topic_validation.hpp>
#include <boost/mqtt5/detail/utf8_mqtt.hpp>

#include <boost/mqtt5/impl/codecs/message_decoders.hpp>
#include <boost/mqtt5/impl/codecs/message_encoders.hpp>
#include <boost/mqtt5/impl/disconnect_op.hpp>

#include <boost/asio/associated_allocator.hpp>
#include <boost/asio/associated_executor.hpp>
#include <boost/asio/cancellation_type.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/prepend.hpp>

#include <cstdint>
#include <memory>
#include <string>
#include <type_traits>

namespace boost::mqtt5::detail {

namespace asio = boost::asio;

template <qos_e qos_type>
using on_publish_signature = std::conditional_t<
    qos_type == qos_e::at_most_once,
        void (error_code),
        std::conditional_t<
            qos_type == qos_e::at_least_once,
                void (error_code, reason_code, puback_props),
                void (error_code, reason_code, pubcomp_props)
        >
>;

template <qos_e qos_type>
using on_publish_props_type = std::conditional_t<
    qos_type == qos_e::at_most_once,
        void,
        std::conditional_t<
            qos_type == qos_e::at_least_once,
                puback_props,
                pubcomp_props
        >
>;

template <typename ClientService, typename Handler, qos_e qos_type>
class publish_send_op {
    using client_service = ClientService;

    struct on_publish {};
    struct on_puback {};
    struct on_pubrec {};
    struct on_pubrel {};
    struct on_pubcomp {};

    std::shared_ptr<client_service> _svc_ptr;

    using handler_type = cancellable_handler<
        Handler,
        typename client_service::executor_type
    >;
    handler_type _handler;

    serial_num_t _serial_num;

    struct validation_context {
        retain_e retain;
        uint16_t topic_alias;
    } _validation_ctx;

public:
    publish_send_op(
        std::shared_ptr<client_service> svc_ptr,
        Handler&& handler
    ) :
        _svc_ptr(std::move(svc_ptr)),
        _handler(std::move(handler), _svc_ptr->get_executor())
    {
        auto slot = asio::get_associated_cancellation_slot(_handler);
        if (slot.is_connected())
            slot.assign([&svc = *_svc_ptr](asio::cancellation_type_t) {
                svc.cancel();
            });
    }

    publish_send_op(publish_send_op&&) = default;
    publish_send_op(const publish_send_op&) = delete;

    publish_send_op& operator=(publish_send_op&&) = default;
    publish_send_op& operator=(const publish_send_op&) = delete;

    using allocator_type = asio::associated_allocator_t<handler_type>;
    allocator_type get_allocator() const noexcept {
        return asio::get_associated_allocator(_handler);
    }

    using executor_type = typename client_service::executor_type;
    executor_type get_executor() const noexcept {
        return _svc_ptr->get_executor();
    }

    void perform(
        std::string topic, std::string payload,
        retain_e retain, const publish_props& props
    ) {
        uint16_t packet_id = 0;
        if constexpr (qos_type != qos_e::at_most_once) {
            packet_id = _svc_ptr->allocate_pid();
            if (packet_id == 0)
                return complete_immediate(client::error::pid_overrun, packet_id);
        }

        _validation_ctx = { retain, props[prop::topic_alias].value_or(0) };

        if (auto ec = validate_publish(topic, payload, props))
            return complete_immediate(ec, packet_id);

        auto publish = control_packet<allocator_type>::of(
            with_pid, get_allocator(),
            encoders::encode_publish, packet_id,
            std::move(topic), std::move(payload),
            qos_type, retain, dup_e::no, props
        );

        if (_svc_ptr->was_connected())
            if (auto ec = validate_with_connack_props(publish.size()))
                return complete_immediate(ec, publish.packet_id());

        _serial_num = _svc_ptr->next_serial_num();
        send_publish(std::move(publish));
    }

    void send_publish(control_packet<allocator_type> publish) {
        auto wire_data = publish.wire_data();
        _svc_ptr->async_send(
            wire_data,
            _serial_num,
            send_flag::throttled * (qos_type != qos_e::at_most_once),
            asio::prepend(std::move(*this), on_publish {}, std::move(publish))
        );
    }

    void resend_publish(control_packet<allocator_type> publish) {
        if (_handler.cancelled() != asio::cancellation_type_t::none)
            return complete(
                asio::error::operation_aborted, publish.packet_id()
            );

        if (auto ec = validate_with_connack_props(publish.size()))
            return complete(ec, publish.packet_id());

        send_publish(std::move(publish));
    }

    void operator()(
        on_publish, control_packet<allocator_type> publish,
        error_code ec
    ) {
        if (ec == asio::error::try_again)
            return resend_publish(std::move(publish));

        if constexpr (qos_type == qos_e::at_most_once)
            return complete(ec);

        else {
            auto packet_id = publish.packet_id();

            if (ec)
                return complete(ec, packet_id);

            if constexpr (qos_type == qos_e::at_least_once)
                _svc_ptr->async_wait_reply(
                    control_code_e::puback, packet_id,
                    asio::prepend(
                        std::move(*this), on_puback {}, std::move(publish)
                    )
                );

            else if constexpr (qos_type == qos_e::exactly_once)
                _svc_ptr->async_wait_reply(
                    control_code_e::pubrec, packet_id,
                    asio::prepend(
                        std::move(*this), on_pubrec {}, std::move(publish)
                    )
                );
        }
    }


    template <
        qos_e q = qos_type,
        std::enable_if_t<q == qos_e::at_least_once, bool> = true
    >
    void operator()(
        on_puback, control_packet<allocator_type> publish,
        error_code ec, byte_citer first, byte_citer last
    ) {
        if (ec == asio::error::try_again) // "resend unanswered"
            return resend_publish(std::move(publish.set_dup()));

        uint16_t packet_id = publish.packet_id();

        if (ec)
            return complete(ec, packet_id);

        auto puback = decoders::decode_puback(
            static_cast<uint32_t>(std::distance(first, last)), first
        );
        if (!puback.has_value()) {
            on_malformed_packet("Malformed PUBACK: cannot decode");
            return resend_publish(std::move(publish.set_dup()));
        }

        auto& [reason_code, props] = *puback;
        auto rc = to_reason_code<reason_codes::category::puback>(reason_code);
        if (!rc) {
            on_malformed_packet("Malformed PUBACK: invalid Reason Code");
            return resend_publish(std::move(publish.set_dup()));
        }

        complete(ec, packet_id, *rc, std::move(props));
    }

    template <
        qos_e q = qos_type,
        std::enable_if_t<q == qos_e::exactly_once, bool> = true
    >
    void operator()(
        on_pubrec, control_packet<allocator_type> publish,
        error_code ec, byte_citer first, byte_citer last
    ) {
        if (ec == asio::error::try_again) // "resend unanswered"
            return resend_publish(std::move(publish.set_dup()));

        uint16_t packet_id = publish.packet_id();

        if (ec)
            return complete(ec, packet_id);

        auto pubrec = decoders::decode_pubrec(
            static_cast<uint32_t>(std::distance(first, last)), first
        );
        if (!pubrec.has_value()) {
            on_malformed_packet("Malformed PUBREC: cannot decode");
            return resend_publish(std::move(publish.set_dup()));
        }

        auto& [reason_code, props] = *pubrec;

        auto rc = to_reason_code<reason_codes::category::pubrec>(reason_code);
        if (!rc) {
            on_malformed_packet("Malformed PUBREC: invalid Reason Code");
            return resend_publish(std::move(publish.set_dup()));
        }

        if (*rc)
            return complete(ec, packet_id, *rc);

        auto pubrel = control_packet<allocator_type>::of(
            with_pid, get_allocator(),
            encoders::encode_pubrel, packet_id,
            0, pubrel_props {}
        );

        send_pubrel(std::move(pubrel), false);
    }

    void send_pubrel(control_packet<allocator_type> pubrel, bool throttled) {
        auto wire_data = pubrel.wire_data();
        _svc_ptr->async_send(
            wire_data,
            _serial_num,
            (send_flag::throttled * throttled) | send_flag::prioritized,
            asio::prepend(std::move(*this), on_pubrel {}, std::move(pubrel))
        );
    }

    template <
        qos_e q = qos_type,
        std::enable_if_t<q == qos_e::exactly_once, bool> = true
    >
    void operator()(
        on_pubrel, control_packet<allocator_type> pubrel, error_code ec
    ) {
        if (ec == asio::error::try_again)
            return send_pubrel(std::move(pubrel), true);

        uint16_t packet_id = pubrel.packet_id();

        if (ec)
            return complete(ec, packet_id);

        _svc_ptr->async_wait_reply(
            control_code_e::pubcomp, packet_id,
            asio::prepend(std::move(*this), on_pubcomp {}, std::move(pubrel))
        );
    }

    template <
        qos_e q = qos_type,
        std::enable_if_t<q == qos_e::exactly_once, bool> = true
    >
    void operator()(
        on_pubcomp, control_packet<allocator_type> pubrel,
        error_code ec,
        byte_citer first, byte_citer last
    ) {
        if (ec == asio::error::try_again) // "resend unanswered"
            return send_pubrel(std::move(pubrel), true);

        uint16_t packet_id = pubrel.packet_id();

        if (ec)
            return complete(ec, packet_id);

        auto pubcomp = decoders::decode_pubcomp(
            static_cast<uint32_t>(std::distance(first, last)), first
        );
        if (!pubcomp.has_value()) {
            on_malformed_packet("Malformed PUBCOMP: cannot decode");
            return send_pubrel(std::move(pubrel), true);
        }

        auto& [reason_code, props] = *pubcomp;

        auto rc = to_reason_code<reason_codes::category::pubcomp>(reason_code);
        if (!rc) {
            on_malformed_packet("Malformed PUBCOMP: invalid Reason Code");
            return send_pubrel(std::move(pubrel), true);
        }

        return complete(ec, pubrel.packet_id(), *rc);
    }

private:

    static error_code validate_publish(
        const std::string& topic, const std::string& payload,
        const publish_props& props
    ) {
        constexpr uint8_t default_payload_format_ind = 0;

        auto topic_name_valid = props[prop::topic_alias].has_value() ?
            validate_topic_alias_name(topic) == validation_result::valid :
            validate_topic_name(topic) == validation_result::valid
        ;

        if (!topic_name_valid)
            return client::error::invalid_topic;

        auto payload_format_ind = props[prop::payload_format_indicator]
            .value_or(default_payload_format_ind);
        if (
            payload_format_ind == 1 &&
            validate_mqtt_utf8(payload) != validation_result::valid
        )
            return client::error::malformed_packet;

        return validate_props(props);
    }

    static error_code validate_props(const publish_props& props) {
        if (props[prop::topic_alias] && *props[prop::topic_alias] == 0)
            return client::error::malformed_packet;

        const auto& response_topic = props[prop::response_topic];
        if (
            response_topic &&
            validate_topic_name(*response_topic) != validation_result::valid
        )
            return client::error::malformed_packet;

        const auto& user_properties = props[prop::user_property];
        for (const auto& user_property: user_properties)
            if (!is_valid_string_pair(user_property))
                return client::error::malformed_packet;

        if (!props[prop::subscription_identifier].empty())
            return client::error::malformed_packet;

        const auto& content_type = props[prop::content_type];
        if (
            content_type &&
            validate_mqtt_utf8(*content_type) != validation_result::valid
        )
            return client::error::malformed_packet;

        return {};
    }

    error_code validate_with_connack_props(size_t packet_size) const {
        constexpr uint16_t default_topic_alias_max = 0;
        constexpr uint8_t default_maximum_qos = 2;
        constexpr uint8_t default_retain_available = 1;

        auto topic_alias_max =
            _svc_ptr->connack_property(prop::topic_alias_maximum)
                .value_or(default_topic_alias_max);
        if (_validation_ctx.topic_alias > topic_alias_max)
            return client::error::topic_alias_maximum_reached;

        auto max_qos = _svc_ptr->connack_property(prop::maximum_qos)
            .value_or(default_maximum_qos);
        auto retain_available =
            _svc_ptr->connack_property(prop::retain_available)
                .value_or(default_retain_available);

        if (uint8_t(qos_type) > max_qos)
            return client::error::qos_not_supported;

        if (retain_available == 0 && _validation_ctx.retain == retain_e::yes)
            return client::error::retain_not_available;

        if (packet_size > _svc_ptr->max_packet_size())
            return client::error::packet_too_large;

        return {};
    }

    void on_malformed_packet(const std::string& reason) {
        auto props = disconnect_props {};
        props[prop::reason_string] = reason;
        async_disconnect(
            disconnect_rc_e::malformed_packet, props, _svc_ptr,
            asio::detached
        );
    }

    template <
        qos_e q = qos_type,
        std::enable_if_t<q == qos_e::at_most_once, bool> = true
    >
    void complete(error_code ec, uint16_t = 0) {
        _handler.complete(ec);
    }

    template <
        qos_e q = qos_type,
        std::enable_if_t<q == qos_e::at_most_once, bool> = true
    >
    void complete_immediate(error_code ec, uint16_t) {
        _handler.complete_immediate(ec);
    }

    template <
        typename Props = on_publish_props_type<qos_type>,
        std::enable_if_t<
            std::is_same_v<Props, puback_props> ||
            std::is_same_v<Props, pubcomp_props>,
            bool
        > = true
    >
    void complete(
        error_code ec, uint16_t packet_id,
        reason_code rc = reason_codes::empty, Props&& props = Props {}
    ) {
        _svc_ptr->free_pid(packet_id, true);
        _handler.complete(ec, rc, std::forward<Props>(props));
    }

    template <
        typename Props = on_publish_props_type<qos_type>,
        std::enable_if_t<
            std::is_same_v<Props, puback_props> ||
            std::is_same_v<Props, pubcomp_props>,
            bool
        > = true
    >
    void complete_immediate(error_code ec, uint16_t packet_id) {
        if (packet_id != 0)
            _svc_ptr->free_pid(packet_id, false);
        _handler.complete_immediate(ec, reason_codes::empty, Props {});
    }
};

template <typename ClientService, qos_e qos_type>
class initiate_async_publish {
    std::shared_ptr<ClientService> _svc_ptr;
public:
    explicit initiate_async_publish(std::shared_ptr<ClientService> svc_ptr) :
        _svc_ptr(std::move(svc_ptr))
    {}

    using executor_type = typename ClientService::executor_type;
    executor_type get_executor() const noexcept {
        return _svc_ptr->get_executor();
    }

    template <typename Handler>
    void operator()(
        Handler&& handler,
        std::string topic, std::string payload,
        retain_e retain, const publish_props& props
    ) {
        detail::publish_send_op<ClientService, Handler, qos_type> {
            _svc_ptr, std::move(handler) 
        }.perform(
            std::move(topic), std::move(payload), retain, props
        );
    }
};

} // end namespace boost::mqtt5::detail

#endif // !BOOST_MQTT5_PUBLISH_SEND_OP_HPP
