//
// Copyright (c) 2023-2026 Ivica Siladic, Bruno Iljazovic, Korina Simicevic
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MQTT5_RUN_OP_HPP
#define BOOST_MQTT5_RUN_OP_HPP

#include <boost/mqtt5/error.hpp>

#include <boost/mqtt5/detail/cancellable_handler.hpp>
#include <boost/mqtt5/detail/control_packet.hpp>
#include <boost/mqtt5/detail/internal_types.hpp>

#include <boost/mqtt5/impl/ping_op.hpp>
#include <boost/mqtt5/impl/read_message_op.hpp>
#include <boost/mqtt5/impl/sentry_op.hpp>

#include <boost/asio/associated_allocator.hpp>
#include <boost/asio/associated_cancellation_slot.hpp>
#include <boost/asio/associated_executor.hpp>
#include <boost/asio/experimental/parallel_group.hpp>

#include <memory>

namespace boost::mqtt5::detail {

namespace asio = boost::asio;

template <typename ClientService, typename Handler>
class run_op {
    using client_service = ClientService;

    std::shared_ptr<client_service> _svc_ptr;

    using handler_type = cancellable_handler<
        Handler,
        typename client_service::executor_type
    >;
    handler_type _handler;

public:
    run_op(
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

    run_op(run_op&&) = default;
    run_op(const run_op&) = delete;

    run_op& operator=(run_op&&) = default;
    run_op& operator=(const run_op&) = delete;

    using allocator_type = asio::associated_allocator_t<handler_type>;
    allocator_type get_allocator() const noexcept {
        return asio::get_associated_allocator(_handler);
    }

    using executor_type = typename client_service::executor_type;
    executor_type get_executor() const noexcept {
        return _svc_ptr->get_executor();
    }

    void perform() {
        namespace asioex = boost::asio::experimental;

        if (_svc_ptr->is_open())
            return _handler.complete_immediate(
                error_code(client::error::already_running)
            );

        _svc_ptr->_stream.open();
        _svc_ptr->_rec_channel.reset();

        auto init_read_message_op = [](
            auto handler, std::shared_ptr<client_service> svc_ptr
        ) {
            return read_message_op { std::move(svc_ptr), std::move(handler) }
                .perform();
        };

        auto init_ping_op = [](
            auto handler, std::shared_ptr<client_service> svc_ptr
        ) {
            return ping_op { std::move(svc_ptr), std::move(handler) }
                .perform();
        };

        auto init_senty_op = [](
            auto handler, std::shared_ptr<client_service> svc_ptr
        ) {
            return sentry_op { std::move(svc_ptr), std::move(handler) }
                .perform();
        };

        asioex::make_parallel_group(
            asio::async_initiate<const asio::deferred_t, void ()>(
                init_read_message_op, asio::deferred, _svc_ptr
            ),
            asio::async_initiate<const asio::deferred_t, void ()>(
                init_ping_op, asio::deferred, _svc_ptr
            ),
            asio::async_initiate<const asio::deferred_t, void ()>(
                init_senty_op, asio::deferred, _svc_ptr
            )
        ).async_wait(asioex::wait_for_all(), std::move(*this));
    }

    void operator()(std::array<std::size_t, 3> /* ord */) {
        _handler.complete(make_error_code(asio::error::operation_aborted));
    }
};

template <typename ClientService>
class initiate_async_run {
    std::shared_ptr<ClientService> _svc_ptr;
public:
    explicit initiate_async_run(std::shared_ptr<ClientService> svc_ptr) :
        _svc_ptr(std::move(svc_ptr))
    {}

    using executor_type = typename ClientService::executor_type;
    executor_type get_executor() const noexcept {
        return _svc_ptr->get_executor();
    }

    template <typename Handler>
    void operator()(Handler&& handler) {
        run_op<ClientService, Handler> {
            _svc_ptr, std::move(handler) 
        }.perform();
    }
};


} // end namespace boost::mqtt5::detail

#endif // !BOOST_MQTT5_RUN_OP_HPP
