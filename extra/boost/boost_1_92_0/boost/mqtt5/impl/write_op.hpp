//
// Copyright (c) 2023-2026 Ivica Siladic, Bruno Iljazovic, Korina Simicevic
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MQTT5_WRITE_OP_HPP
#define BOOST_MQTT5_WRITE_OP_HPP

#include <boost/mqtt5/detail/async_traits.hpp>

#include <boost/asio/associated_allocator.hpp>
#include <boost/asio/associated_executor.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/prepend.hpp>
#include <boost/asio/write.hpp>
#include <boost/system/error_code.hpp>

namespace boost::mqtt5::detail {

template <typename Owner, typename Handler>
class write_op {
    struct on_write {};
    struct on_reconnect {};

    Owner& _owner;

    using handler_type = Handler;
    handler_type _handler;

public:
    write_op(Owner& owner, Handler&& handler) :
        _owner(owner), _handler(std::move(handler))
    {}

    write_op(write_op&&) = default;
    write_op(const write_op&) = delete;

    write_op& operator=(write_op&&) = default;
    write_op& operator=(const write_op&) = delete;

    using allocator_type = asio::associated_allocator_t<handler_type>;
    allocator_type get_allocator() const noexcept {
        return asio::get_associated_allocator(_handler);
    }

    using executor_type = asio::associated_executor_t<handler_type>;
    executor_type get_executor() const noexcept {
        return asio::get_associated_executor(_handler);
    }

    template <typename BufferType>
    void perform(BufferType& buffer) {
        auto stream_ptr = _owner._stream_ptr;
        if (_owner.was_connected())
            // note: write operation should not be time-limited
            detail::async_write(
                *stream_ptr, buffer,
                asio::prepend(std::move(*this), on_write {}, stream_ptr)
            );
        else
            _owner.async_reconnect(
                stream_ptr, asio::prepend(std::move(*this), on_reconnect {})
            );
    }

    void operator()(
        on_write, typename Owner::stream_ptr stream_ptr,
        error_code ec, size_t bytes_written
    ) {
        if (!_owner.is_open())
            return complete(asio::error::operation_aborted, 0);

        if (!ec)
            return complete(ec, bytes_written);

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
    void complete(error_code ec, size_t bytes_written) {
        std::move(_handler)(ec, bytes_written);
    }
};

} // end namespace boost::mqtt5::detail

#endif // !BOOST_MQTT5_WRITE_OP_HPP
