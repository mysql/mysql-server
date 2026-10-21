//
// Copyright (c) 2023-2026 Ivica Siladic, Bruno Iljazovic, Korina Simicevic
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MQTT5_SUBSCRIBE_OP_HPP
#define BOOST_MQTT5_SUBSCRIBE_OP_HPP

#include <boost/mqtt5/error.hpp>
#include <boost/mqtt5/reason_codes.hpp>
#include <boost/mqtt5/types.hpp>

#include <boost/mqtt5/detail/cancellable_handler.hpp>
#include <boost/mqtt5/detail/control_packet.hpp>
#include <boost/mqtt5/detail/internal_types.hpp>
#include <boost/mqtt5/detail/topic_validation.hpp>

#include <boost/mqtt5/impl/codecs/message_decoders.hpp>
#include <boost/mqtt5/impl/codecs/message_encoders.hpp>
#include <boost/mqtt5/impl/disconnect_op.hpp>

#include <boost/asio/associated_allocator.hpp>
#include <boost/asio/associated_cancellation_slot.hpp>
#include <boost/asio/associated_executor.hpp>
#include <boost/asio/cancellation_type.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/prepend.hpp>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace boost::mqtt5::detail {

namespace asio = boost::asio;

template <typename ClientService, typename Handler>
class subscribe_op {
    using client_service = ClientService;

    struct on_subscribe {};
    struct on_suback {};

    std::shared_ptr<client_service> _svc_ptr;

    using handler_type = cancellable_handler<
        Handler,
        typename client_service::executor_type
    >;
    handler_type _handler;

    size_t _num_topics { 0 };

    struct validation_context {
        bool has_wildcard { false }, is_shared { false }, has_subid { false };
    } _validation_ctx;

public:
    subscribe_op(
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

    subscribe_op(subscribe_op&&) = default;
    subscribe_op(const subscribe_op&) = delete;

    subscribe_op& operator=(subscribe_op&&) = default;
    subscribe_op& operator=(const subscribe_op&) = delete;

    using allocator_type = asio::associated_allocator_t<handler_type>;
    allocator_type get_allocator() const noexcept {
        return asio::get_associated_allocator(_handler);
    }

    using executor_type = typename client_service::executor_type;
    executor_type get_executor() const noexcept {
        return _svc_ptr->get_executor();
    }

    void perform(
        const std::vector<subscribe_topic>& topics,
        const subscribe_props& props
    ) {
        _num_topics = topics.size();

        uint16_t packet_id = _svc_ptr->allocate_pid();
        if (packet_id == 0)
            return complete_immediate(client::error::pid_overrun, packet_id);

        if (_num_topics == 0)
            return complete_immediate(client::error::invalid_topic, packet_id);

        if (auto ec = validate_subscribe(topics, props, _validation_ctx))
            return complete_immediate(ec, packet_id);

        auto subscribe = control_packet<allocator_type>::of(
            with_pid, get_allocator(),
            encoders::encode_subscribe, packet_id,
            topics, props
        );

        if (_svc_ptr->was_connected())
            if (auto ec = validate_with_connack_props(subscribe.size()))
                return complete_immediate(ec, packet_id);

        send_subscribe(std::move(subscribe));
    }

    void send_subscribe(control_packet<allocator_type> subscribe) {
        auto wire_data = subscribe.wire_data();
        _svc_ptr->async_send(
            wire_data,
            no_serial, send_flag::none,
            asio::prepend(
                std::move(*this), on_subscribe {}, std::move(subscribe)
            )
        );
    }

    void resend_subscribe(control_packet<allocator_type> subscribe) {
        if (_handler.cancelled() != asio::cancellation_type_t::none)
            return complete(
                asio::error::operation_aborted, subscribe.packet_id()
            );

        if (auto ec = validate_with_connack_props(subscribe.size()))
            return complete(ec, subscribe.packet_id());

        send_subscribe(std::move(subscribe));
    }

    void operator()(
        on_subscribe, control_packet<allocator_type> packet,
        error_code ec
    ) {
        if (ec == asio::error::try_again)
            return resend_subscribe(std::move(packet));

        auto packet_id = packet.packet_id();

        if (ec)
            return complete(ec, packet_id);

        _svc_ptr->async_wait_reply(
            control_code_e::suback, packet_id,
            asio::prepend(std::move(*this), on_suback {}, std::move(packet))
        );
    }

    void operator()(
        on_suback, control_packet<allocator_type> packet,
        error_code ec, byte_citer first, byte_citer last
    ) {
        if (ec == asio::error::try_again) // "resend unanswered"
            return resend_subscribe(std::move(packet));

        uint16_t packet_id = packet.packet_id();

        if (ec)
            return complete(ec, packet_id);

        auto suback = decoders::decode_suback(
            static_cast<uint32_t>(std::distance(first, last)), first
        );
        if (!suback.has_value()) {
            on_malformed_packet("Malformed SUBACK: cannot decode");
            return resend_subscribe(std::move(packet));
        }

        auto& [props, rcs] = *suback;
        auto reason_codes = to_reason_codes(std::move(rcs));
        if (reason_codes.size() != _num_topics) {
            on_malformed_packet(
                "Malformed SUBACK: does not contain a "
                "valid Reason Code for every Topic Filter"
            );
            return resend_subscribe(std::move(packet));
        }

        complete(
            ec, packet_id, std::move(reason_codes), std::move(props)
        );
    }

private:

    error_code validate_with_connack_props(size_t packet_size) const {
        if (packet_size > _svc_ptr->max_packet_size())
            return client::error::packet_too_large;

        auto wildcard_available = _svc_ptr->connack_property(
            prop::wildcard_subscription_available
        ).value_or(1);
        if (!wildcard_available && _validation_ctx.has_wildcard)
            return client::error::wildcard_subscription_not_available;

        auto shared_available = _svc_ptr->connack_property(
            prop::shared_subscription_available
        ).value_or(1);
        if (!shared_available && _validation_ctx.is_shared)
            return client::error::shared_subscription_not_available;

        auto sub_id_available = _svc_ptr->connack_property(
            prop::subscription_identifier_available
        ).value_or(1);
        if (!sub_id_available && _validation_ctx.has_subid)
            return client::error::subscription_identifier_not_available;

        return {};
    }

    static error_code validate_subscribe(
        const std::vector<subscribe_topic>& topics,
        const subscribe_props& props, validation_context& ctx
    ) {
        error_code ec;
        for (const auto& topic: topics) {
            ec = validate_topic(topic, ctx);
            if (ec)
                return ec;
        }

        ec = validate_props(props, ctx);
        return ec;
    }

    static error_code validate_topic(
        const subscribe_topic& topic, validation_context& ctx
    ) {
        std::string_view topic_filter = topic.topic_filter;

        validation_result result = validation_result::valid;
        if (
            topic_filter.compare(0, shared_sub_prefix.size(), shared_sub_prefix) == 0
        ) {
            ctx.is_shared = true;
            result = validate_shared_topic_filter(topic_filter);
        } else
            result = validate_topic_filter(topic_filter);

        if (result == validation_result::invalid)
            return client::error::invalid_topic;

        if (result == validation_result::has_wildcard_character)
            ctx.has_wildcard = true;

        return error_code {};
    }

    static error_code validate_props(
        const subscribe_props& props, validation_context& ctx
    ) {
        const auto& user_properties = props[prop::user_property];
        for (const auto& user_property: user_properties)
            if (!is_valid_string_pair(user_property))
                return client::error::malformed_packet;

        const auto& sub_id = props[prop::subscription_identifier];
        if (!sub_id.has_value())
            return error_code {};

        ctx.has_subid = true;

        return (min_subscription_identifier <= *sub_id &&
            *sub_id <= max_subscription_identifier) ?
                error_code {} :
                client::error::malformed_packet;
    }

    static std::vector<reason_code> to_reason_codes(std::vector<uint8_t> codes) {
        std::vector<reason_code> ret;
        for (uint8_t code : codes) {
            auto rc = to_reason_code<reason_codes::category::suback>(code);
            if (rc)
                ret.push_back(*rc);
        }
        return ret;
    }

    void on_malformed_packet(const std::string& reason) {
        auto props = disconnect_props {};
        props[prop::reason_string] = reason;
        async_disconnect(
            disconnect_rc_e::malformed_packet, props, _svc_ptr,
            asio::detached
        );
    }

    void complete_immediate(error_code ec, uint16_t packet_id) {
        if (packet_id != 0)
            _svc_ptr->free_pid(packet_id);
        _handler.complete_immediate(
            ec, std::vector<reason_code>(_num_topics, reason_codes::empty),
            suback_props {}
        );
    }

    void complete(
        error_code ec, uint16_t packet_id,
        std::vector<reason_code> reason_codes = {}, suback_props props = {}
    ) {
        if (reason_codes.empty() && _num_topics)
            reason_codes = std::vector<reason_code>(_num_topics, reason_codes::empty);

        if (!_svc_ptr->subscriptions_present()) {
            bool has_success_rc = std::any_of(
                reason_codes.cbegin(), reason_codes.cend(),
                [](const reason_code& rc) { return !rc; }
            );
            if (has_success_rc)
                _svc_ptr->subscriptions_present(true);
        }

        _svc_ptr->free_pid(packet_id);
        _handler.complete(ec, std::move(reason_codes), std::move(props));
    }
};

template <typename ClientService>
class initiate_async_subscribe {
    std::shared_ptr<ClientService> _svc_ptr;
public:
    explicit initiate_async_subscribe(std::shared_ptr<ClientService> svc_ptr) :
        _svc_ptr(std::move(svc_ptr))
    {}

    using executor_type = typename ClientService::executor_type;
    executor_type get_executor() const noexcept {
        return _svc_ptr->get_executor();
    }

    template <typename Handler>
    void operator()(
        Handler&& handler,
        const std::vector<subscribe_topic>& topics, const subscribe_props& props
    ) {
        detail::subscribe_op { _svc_ptr, std::move(handler) }
            .perform(topics, props);
    }
};

} // end namespace boost::mqtt5::detail

#endif // !BOOST_MQTT5_SUBSCRIBE_OP_HPP
