//
// Copyright (c) 2023-2026 Ivica Siladic, Bruno Iljazovic, Korina Simicevic
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MQTT5_DISCONNECT_OP_HPP
#define BOOST_MQTT5_DISCONNECT_OP_HPP

#include <boost/mqtt5/types.hpp>

#include <boost/mqtt5/detail/cancellable_handler.hpp>
#include <boost/mqtt5/detail/control_packet.hpp>
#include <boost/mqtt5/detail/internal_types.hpp>
#include <boost/mqtt5/detail/topic_validation.hpp>
#include <boost/mqtt5/detail/utf8_mqtt.hpp>

#include <boost/mqtt5/impl/codecs/message_encoders.hpp>

#include <boost/asio/any_completion_handler.hpp>
#include <boost/asio/associated_allocator.hpp>
#include <boost/asio/associated_cancellation_slot.hpp>
#include <boost/asio/associated_executor.hpp>
#include <boost/asio/async_result.hpp>
#include <boost/asio/consign.hpp>
#include <boost/asio/deferred.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/experimental/parallel_group.hpp>
#include <boost/asio/prepend.hpp>

#include <cstdint>
#include <memory>

namespace boost::mqtt5::detail {

namespace asio = boost::asio;

template <
    typename ClientService,
    typename DisconnectContext
>
class disconnect_op {
    using client_service = ClientService;

    struct on_disconnect {};
    struct on_shutdown {};

    std::shared_ptr<client_service> _svc_ptr;
    DisconnectContext _context;

    using handler_type = cancellable_handler<
        asio::any_completion_handler<void (error_code)>,
        typename ClientService::executor_type
    >;
    handler_type _handler;

public:
    template <typename Handler>
    disconnect_op(
        std::shared_ptr<client_service> svc_ptr,
        DisconnectContext&& context, Handler&& handler
    ) :
        _svc_ptr(std::move(svc_ptr)), _context(std::move(context)),
        _handler(std::move(handler), _svc_ptr->get_executor())
    {
        auto slot = asio::get_associated_cancellation_slot(_handler);
        if (slot.is_connected())
            slot.assign([&svc = *_svc_ptr](asio::cancellation_type_t) {
                svc.cancel();
            });
    }

    disconnect_op(disconnect_op&&) = default;
    disconnect_op(const disconnect_op&) = delete;

    disconnect_op& operator=(disconnect_op&&) = default;
    disconnect_op& operator=(const disconnect_op&) = delete;

    using allocator_type = asio::associated_allocator_t<handler_type>;
    allocator_type get_allocator() const noexcept {
        return asio::get_associated_allocator(_handler);
    }

    using executor_type = typename client_service::executor_type;
    executor_type get_executor() const noexcept {
        return _svc_ptr->get_executor();
    }

    void perform() {
        error_code ec = validate_disconnect(_context.props);
        if (ec)
            return complete_immediate(ec);

        auto disconnect = control_packet<allocator_type>::of(
            no_pid, get_allocator(),
            encoders::encode_disconnect,
            static_cast<uint8_t>(_context.reason_code), _context.props
        );

        send_disconnect(std::move(disconnect));
    }

    void send_disconnect(control_packet<allocator_type> disconnect) {
        if (disconnect.size() > _svc_ptr->max_packet_size())
            // drop properties
            disconnect = control_packet<allocator_type>::of(
                no_pid, get_allocator(),
                encoders::encode_disconnect,
                static_cast<uint8_t>(_context.reason_code), disconnect_props {}
            );

        auto wire_data = disconnect.wire_data();
        _svc_ptr->async_send(
            wire_data,
            no_serial, send_flag::terminal,
            asio::prepend(
                std::move(*this),
                on_disconnect {}, std::move(disconnect)
            )
        );
    }

    void operator()(
        on_disconnect,
        control_packet<allocator_type> disconnect, error_code ec
    ) {
        // The connection must be closed even
        // if we failed to send the DISCONNECT packet
        // with Reason Code of 0x80 or greater.

        if (
            ec == asio::error::operation_aborted ||
            ec == asio::error::no_recovery
        )
            return complete(asio::error::operation_aborted);

        if (ec == asio::error::try_again) {
            if (_context.terminal)
                return send_disconnect(std::move(disconnect));
            return complete(error_code {});
        }

        return _svc_ptr->async_shutdown(
            asio::prepend(std::move(*this), on_shutdown {})
        );
    }

    void operator()(on_shutdown, error_code ec) {
        if (_context.terminal)
            _svc_ptr->cancel();
        complete(ec);
    }

private:
    static error_code validate_disconnect(const disconnect_props& props) {
        const auto& reason_string = props[prop::reason_string];
        if (
            reason_string &&
            validate_mqtt_utf8(*reason_string) != validation_result::valid
        )
            return client::error::malformed_packet;

        const auto& user_properties = props[prop::user_property];
        for (const auto& user_property: user_properties)
            if (!is_valid_string_pair(user_property))
                return client::error::malformed_packet;
        return error_code {};
    }

    void complete(error_code ec) {
        _handler.complete(ec);
    }

    void complete_immediate(error_code ec) {
        _handler.complete_immediate(ec);
    }
};

template <typename ClientService, typename Handler>
class terminal_disconnect_op {
    using client_service = ClientService;

    static constexpr uint8_t seconds = 5;

    std::shared_ptr<client_service> _svc_ptr;
    std::unique_ptr<timer_type> _timer;

    using handler_type = Handler;
    handler_type _handler;

public:
    terminal_disconnect_op(
        std::shared_ptr<client_service> svc_ptr,
        Handler&& handler
    ) :
        _svc_ptr(std::move(svc_ptr)),
        _timer(new timer_type(_svc_ptr->get_executor())),
        _handler(std::move(handler))
    {}

    terminal_disconnect_op(terminal_disconnect_op&&) = default;
    terminal_disconnect_op(const terminal_disconnect_op&) = delete;

    terminal_disconnect_op& operator=(terminal_disconnect_op&&) = default;
    terminal_disconnect_op& operator=(const terminal_disconnect_op&) = delete;

    using allocator_type = asio::associated_allocator_t<handler_type>;
    allocator_type get_allocator() const noexcept {
        return asio::get_associated_allocator(_handler);
    }

    using cancellation_slot_type = asio::associated_cancellation_slot_t<handler_type>;
    cancellation_slot_type get_cancellation_slot() const noexcept {
        return asio::get_associated_cancellation_slot(_handler);
    }

    using executor_type = asio::associated_executor_t<handler_type>;
    executor_type get_executor() const noexcept {
        return asio::get_associated_executor(_handler);
    }

    template <typename DisconnectContext>
    void perform(DisconnectContext&& context) {
        namespace asioex = boost::asio::experimental;

        auto init_disconnect = [](
            auto handler, disconnect_ctx ctx,
            std::shared_ptr<ClientService> svc_ptr
        ) {
            disconnect_op {
                std::move(svc_ptr), std::move(ctx), std::move(handler)
            }.perform();
        };

        _timer->expires_after(std::chrono::seconds(seconds));

        auto timed_disconnect = asioex::make_parallel_group(
            asio::async_initiate<const asio::deferred_t, void (error_code)>(
                init_disconnect, asio::deferred,
                std::forward<DisconnectContext>(context), _svc_ptr
            ),
            _timer->async_wait(asio::deferred)
        );

        timed_disconnect.async_wait(
            asioex::wait_for_one(), std::move(*this)
        );
    }

    void operator()(
        std::array<std::size_t, 2> /* ord */,
        error_code disconnect_ec, error_code /* timer_ec */
    ) {
        std::move(_handler)(disconnect_ec);
    }
};

template <typename ClientService, bool terminal>
class initiate_async_disconnect {
    std::shared_ptr<ClientService> _svc_ptr;
public:
    explicit initiate_async_disconnect(std::shared_ptr<ClientService> svc_ptr) :
        _svc_ptr(std::move(svc_ptr))
    {}

    using executor_type = typename ClientService::executor_type;
    executor_type get_executor() const noexcept {
        return _svc_ptr->get_executor();
    }

    template <typename Handler>
    void operator()(
        Handler&& handler,
        disconnect_rc_e rc, const disconnect_props& props
    ) {
        auto ctx = disconnect_ctx { rc, props, terminal };
        if constexpr (terminal)
            terminal_disconnect_op { _svc_ptr, std::move(handler) }
                .perform(std::move(ctx));
        else
            disconnect_op { _svc_ptr, std::move(ctx), std::move(handler) }
                .perform();
    }
};

template <typename ClientService, typename CompletionToken>
decltype(auto) async_disconnect(
    disconnect_rc_e reason_code, const disconnect_props& props,
    std::shared_ptr<ClientService> svc_ptr,
    CompletionToken&& token
) {
    using Signature = void (error_code);
    return asio::async_initiate<CompletionToken, Signature>(
        initiate_async_disconnect<ClientService, false>(std::move(svc_ptr)), token,
        reason_code, props
    );
}

template <typename ClientService, typename CompletionToken>
decltype(auto) async_terminal_disconnect(
    disconnect_rc_e reason_code, const disconnect_props& props,
    std::shared_ptr<ClientService> svc_ptr,
    CompletionToken&& token
) {
    using Signature = void (error_code);
    return asio::async_initiate<CompletionToken, Signature>(
        initiate_async_disconnect<ClientService, true>(std::move(svc_ptr)), token,
        reason_code, props
    );
}

} // end namespace boost::mqtt5::detail

#endif // !BOOST_MQTT5_DISCONNECT_HPP
