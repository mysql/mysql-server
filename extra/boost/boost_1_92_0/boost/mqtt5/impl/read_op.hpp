//
// Copyright (c) 2023-2026 Ivica Siladic, Bruno Iljazovic, Korina Simicevic
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MQTT5_READ_OP_HPP
#define BOOST_MQTT5_READ_OP_HPP

#include <boost/mqtt5/detail/internal_types.hpp>

#include <boost/asio/associated_allocator.hpp>
#include <boost/asio/associated_executor.hpp>
#include <boost/asio/deferred.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/experimental/parallel_group.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/prepend.hpp>

#include <array>

namespace boost::mqtt5::detail {

namespace asio = boost::asio;
namespace asioex = boost::asio::experimental;

template <typename Owner, typename Handler>
class read_op {
    struct on_read {};
    struct on_reconnect {};

    Owner& _owner;

    using handler_type = Handler;
    handler_type _handler;

public:
    read_op(Owner& owner, Handler&& handler) :
        _owner(owner), _handler(std::move(handler))
    {}

    read_op(read_op&&) = default;
    read_op(const read_op&) = delete;

    read_op& operator=(read_op&&) = default;
    read_op& operator=(const read_op&) = delete;

    using allocator_type = asio::associated_allocator_t<handler_type>;
    allocator_type get_allocator() const noexcept {
        return asio::get_associated_allocator(_handler);
    }

    using executor_type = asio::associated_executor_t<handler_type>;
    executor_type get_executor() const noexcept {
        return asio::get_associated_executor(_handler);
    }

    template <typename BufferType>
    void perform(
        const BufferType& buffer, duration wait_for
    ) {
        auto stream_ptr = _owner._stream_ptr;

        if (_owner.was_connected()) {
            _owner._read_timer.expires_after(wait_for);

            auto timed_read = asioex::make_parallel_group(
                stream_ptr->async_read_some(buffer, asio::deferred),
                _owner._read_timer.async_wait(asio::deferred)
            );

            timed_read.async_wait(
                asioex::wait_for_one(),
                asio::prepend(std::move(*this), on_read {}, stream_ptr)
            );
        }
        else
            _owner.async_reconnect(
                stream_ptr, asio::prepend(std::move(*this), on_reconnect {})
            );
    }

    void operator()(
        on_read, typename Owner::stream_ptr stream_ptr,
        std::array<std::size_t, 2> ord, error_code read_ec, size_t bytes_read,
        error_code
    ) {
        if (!_owner.is_open())
            return complete(asio::error::operation_aborted, bytes_read);

        error_code ec = ord[0] == 1 ? asio::error::timed_out : read_ec;
        bytes_read = ord[0] == 0 ? bytes_read : 0;

        if (!ec)
            return complete(ec, bytes_read);

        _owner.log().at_transport_error(ec);
        _owner.async_reconnect(
            stream_ptr, asio::prepend(std::move(*this), on_reconnect {})
        );
    }

    void operator()(on_reconnect, error_code ec) {
        if ((ec == asio::error::operation_aborted && _owner.is_open()) || !ec)
            ec = asio::error::try_again;

        return complete(ec, 0);
    }

private:
    void complete(error_code ec, size_t bytes_read) {
        std::move(_handler)(ec, bytes_read);
    }
};


} // end namespace boost::mqtt5::detail

#endif // !BOOST_MQTT5_READ_OP_HPP
