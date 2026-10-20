//
// Copyright (c) 2023-2026 Ivica Siladic, Bruno Iljazovic, Korina Simicevic
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MQTT5_UNSUBSCRIBE_OP_HPP
#define BOOST_MQTT5_UNSUBSCRIBE_OP_HPP

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
class unsubscribe_op {
    using client_service = ClientService;

    struct on_unsubscribe {};
    struct on_unsuback {};

    std::shared_ptr<client_service> _svc_ptr;

    using handler_type = cancellable_handler<
        Handler,
        typename client_service::executor_type
    >;
    handler_type _handler;

    size_t _num_topics { 0 };

public:
    unsubscribe_op(
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

    unsubscribe_op(unsubscribe_op&&) = default;
    unsubscribe_op(const unsubscribe_op&) = delete;

    unsubscribe_op& operator=(unsubscribe_op&&) = default;
    unsubscribe_op& operator=(const unsubscribe_op&) = delete;

    using allocator_type = asio::associated_allocator_t<handler_type>;
    allocator_type get_allocator() const noexcept {
        return asio::get_associated_allocator(_handler);
    }

    using executor_type = typename client_service::executor_type;
    executor_type get_executor() const noexcept {
        return _svc_ptr->get_executor();
    }

    void perform(
        const std::vector<std::string>& topics,
        const unsubscribe_props& props
    ) {
        _num_topics = topics.size();

        uint16_t packet_id = _svc_ptr->allocate_pid();
        if (packet_id == 0)
            return complete_immediate(client::error::pid_overrun, packet_id);

        if (_num_topics == 0)
            return complete_immediate(client::error::invalid_topic, packet_id);

        auto ec = validate_unsubscribe(topics, props);
        if (ec)
            return complete_immediate(ec, packet_id);

        auto unsubscribe = control_packet<allocator_type>::of(
            with_pid, get_allocator(),
            encoders::encode_unsubscribe, packet_id,
            topics, props
        );

        if (unsubscribe.size() > _svc_ptr->max_packet_size())
            return complete_immediate(client::error::packet_too_large, packet_id);

        send_unsubscribe(std::move(unsubscribe));
    }

    void send_unsubscribe(control_packet<allocator_type> unsubscribe) {
        auto wire_data = unsubscribe.wire_data();
        _svc_ptr->async_send(
            wire_data,
            no_serial, send_flag::none,
            asio::prepend(
                std::move(*this), on_unsubscribe {}, std::move(unsubscribe)
            )
        );
    }

    void resend_unsubscribe(control_packet<allocator_type> unsubscribe) {
        if (_handler.cancelled() != asio::cancellation_type_t::none)
            return complete(
                asio::error::operation_aborted, unsubscribe.packet_id()
            );

        if (unsubscribe.size() > _svc_ptr->max_packet_size())
            return complete(
                client::error::packet_too_large, unsubscribe.packet_id()
            );

        send_unsubscribe(std::move(unsubscribe));
    }

    void operator()(
        on_unsubscribe, control_packet<allocator_type> packet,
        error_code ec
    ) {
        if (ec == asio::error::try_again)
            return resend_unsubscribe(std::move(packet));

        auto packet_id = packet.packet_id();

        if (ec)
            return complete(ec, packet_id);

        _svc_ptr->async_wait_reply(
            control_code_e::unsuback, packet_id,
            asio::prepend(std::move(*this), on_unsuback {}, std::move(packet))
        );
    }

    void operator()(
        on_unsuback, control_packet<allocator_type> packet,
        error_code ec, byte_citer first, byte_citer last
    ) {
        if (ec == asio::error::try_again) // "resend unanswered"
            return resend_unsubscribe(std::move(packet));

        uint16_t packet_id = packet.packet_id();

        if (ec)
            return complete(ec, packet_id);

        auto unsuback = decoders::decode_unsuback(
            static_cast<uint32_t>(std::distance(first, last)), first
        );
        if (!unsuback.has_value()) {
            on_malformed_packet("Malformed UNSUBACK: cannot decode");
            return resend_unsubscribe(std::move(packet));
        }

        auto& [props, rcs] = *unsuback;
        auto reason_codes = to_reason_codes(std::move(rcs));
        if (reason_codes.size() != _num_topics) {
            on_malformed_packet(
                "Malformed UNSUBACK: does not contain a "
                "valid Reason Code for every Topic Filter"
            );
            return resend_unsubscribe(std::move(packet));
        }

        complete(
            ec, packet_id, std::move(reason_codes), std::move(props)
        );
    }

private:

    static error_code validate_unsubscribe(
        const std::vector<std::string>& topics,
        const unsubscribe_props& props
    ) {
        for (const auto& topic : topics)
            if (validate_topic_filter(topic) != validation_result::valid)
                return client::error::invalid_topic;

        const auto& user_properties = props[prop::user_property];
        for (const auto& user_property: user_properties)
            if (!is_valid_string_pair(user_property))
                return client::error::malformed_packet;
        return error_code {};
    }

    static std::vector<reason_code> to_reason_codes(std::vector<uint8_t> codes) {
        std::vector<reason_code> ret;
        for (uint8_t code : codes) {
            auto rc = to_reason_code<reason_codes::category::unsuback>(code);
            if (rc)
                ret.push_back(*rc);
        }
        return ret;
    }

    void on_malformed_packet(
        const std::string& reason
    ) {
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
            unsuback_props {}
        );
    }

    void complete(
        error_code ec, uint16_t packet_id,
        std::vector<reason_code> reason_codes = {}, unsuback_props props = {}
    ) {
        if (reason_codes.empty() && _num_topics)
            reason_codes = std::vector<reason_code>(_num_topics, reason_codes::empty);

        _svc_ptr->free_pid(packet_id);
        _handler.complete(ec, std::move(reason_codes), std::move(props));
    }
};

template <typename ClientService>
class initiate_async_unsubscribe {
    std::shared_ptr<ClientService> _svc_ptr;
public:
    explicit initiate_async_unsubscribe(std::shared_ptr<ClientService> svc_ptr) :
        _svc_ptr(std::move(svc_ptr))
    {}

    using executor_type = typename ClientService::executor_type;
    executor_type get_executor() const noexcept {
        return _svc_ptr->get_executor();
    }

    template <typename Handler>
    void operator()(
        Handler&& handler,
        const std::vector<std::string>& topics, const unsubscribe_props& props
    ) {
        detail::unsubscribe_op { _svc_ptr, std::move(handler) }
            .perform(topics, props);
    }
};


} // end namespace boost::mqtt5::detail

#endif // !BOOST_MQTT5_UNSUBSCRIBE_OP_HPP
