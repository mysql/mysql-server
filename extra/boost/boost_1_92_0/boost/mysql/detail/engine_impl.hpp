//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_ENGINE_IMPL_HPP
#define BOOST_MYSQL_DETAIL_ENGINE_IMPL_HPP

#include <boost/mysql/error_code.hpp>

#include <boost/mysql/detail/any_resumable_ref.hpp>
#include <boost/mysql/detail/engine.hpp>
#include <boost/mysql/detail/next_action.hpp>

#include <boost/mysql/impl/internal/coroutine.hpp>

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/cancellation_type.hpp>
#include <boost/asio/compose.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/immediate.hpp>
#include <boost/asio/post.hpp>
#include <boost/assert.hpp>

#include <cstddef>
#include <utility>

namespace boost {
namespace mysql {
namespace detail {

inline asio::mutable_buffer to_buffer(span<std::uint8_t> buff) noexcept
{
    return asio::mutable_buffer(buff.data(), buff.size());
}

inline bool has_terminal_cancellation(asio::cancellation_type_t cancel_type)
{
    return static_cast<bool>(cancel_type & asio::cancellation_type_t::terminal);
}

template <class EngineStream>
struct run_algo_op
{
    int resume_point_{0};
    EngineStream& stream_;
    any_resumable_ref resumable_;
    bool has_done_io_{false};
    error_code stored_ec_;

    run_algo_op(EngineStream& stream, any_resumable_ref algo) noexcept : stream_(stream), resumable_(algo) {}

    template <class Self>
    void operator()(Self& self, error_code io_ec = {}, std::size_t bytes_transferred = 0)
    {
        next_action act;

        switch (resume_point_)
        {
        case 0:

            while (true)
            {
                // If we were cancelled, but the last operation completed successfully,
                // set a cancelled error code so the algorithm exits. This might happen
                // if a cancellation signal is emitted after an intermediate operation succeeded
                // but before the handler was called.
                if (!io_ec && has_terminal_cancellation(self.cancelled()))
                    io_ec = asio::error::operation_aborted;

                // Run the op
                act = resumable_.resume(io_ec, bytes_transferred);
                if (act.is_done())
                {
                    stored_ec_ = act.error();
                    if (!has_done_io_)
                    {
                        BOOST_MYSQL_YIELD(
                            resume_point_,
                            1,
                            asio::async_immediate(stream_.get_executor(), std::move(self))
                        )
                    }
                    self.complete(stored_ec_);
                    return;
                }
                else if (act.type() == next_action_type::read)
                {
                    BOOST_MYSQL_YIELD(
                        resume_point_,
                        2,
                        stream_.async_read_some(
                            to_buffer(act.read_args().buffer),
                            act.read_args().use_ssl,
                            std::move(self)
                        )
                    )
                    has_done_io_ = true;
                }
                else if (act.type() == next_action_type::write)
                {
                    BOOST_MYSQL_YIELD(
                        resume_point_,
                        3,
                        stream_.async_write_some(
                            asio::buffer(act.write_args().buffer),
                            act.write_args().use_ssl,
                            std::move(self)
                        )
                    )
                    has_done_io_ = true;
                }
                else if (act.type() == next_action_type::ssl_handshake)
                {
                    BOOST_MYSQL_YIELD(resume_point_, 4, stream_.async_ssl_handshake(std::move(self)))
                    has_done_io_ = true;
                }
                else if (act.type() == next_action_type::ssl_shutdown)
                {
                    BOOST_MYSQL_YIELD(resume_point_, 5, stream_.async_ssl_shutdown(std::move(self)))
                    has_done_io_ = true;
                }
                else if (act.type() == next_action_type::connect)
                {
                    BOOST_MYSQL_YIELD(
                        resume_point_,
                        6,
                        stream_.async_connect(act.connect_endpoint(), std::move(self))
                    )
                    has_done_io_ = true;
                }
                else
                {
                    BOOST_ASSERT(act.type() == next_action_type::close);
                    stream_.close(io_ec);
                }
            }
        }
    }
};

// EngineStream is an "extended" stream concept, with the following operations:
//    using executor_type = asio::any_io_executor;
//    executor_type get_executor();
//    bool supports_ssl() const;
//    std::size_t read_some(asio::mutable_buffer, bool use_ssl, error_code&);
//    void async_read_some(asio::mutable_buffer, bool use_ssl, CompletinToken&&);
//    std::size_t write_some(asio::const_buffer, bool use_ssl, error_code&);
//    void async_write_some(asio::const_buffer, bool use_ssl, CompletinToken&&);
//    void ssl_handshake(error_code&);
//    void async_ssl_handshake(CompletionToken&&);
//    void ssl_shutdown(error_code&);
//    void async_ssl_shutdown(CompletionToken&&);
//    void connect(const void* server_address, error_code&);
//    void async_connect(const void* server_address, CompletionToken&&);
//    void close(error_code&);
// Async operations are only required to support callback types
// See stream_adaptor for an implementation
template <class EngineStream>
class engine_impl final : public engine
{
    EngineStream stream_;

public:
    template <class... Args>
    engine_impl(Args&&... args) : stream_(std::forward<Args>(args)...)
    {
    }

    EngineStream& stream() { return stream_; }
    const EngineStream& stream() const { return stream_; }

    using executor_type = asio::any_io_executor;
    executor_type get_executor() override final { return stream_.get_executor(); }

    bool supports_ssl() const override final { return stream_.supports_ssl(); }

    void run(any_resumable_ref resumable, error_code& ec) override final
    {
        ec.clear();
        error_code io_ec;
        std::size_t bytes_transferred = 0;

        while (true)
        {
            // Run the op
            auto act = resumable.resume(io_ec, bytes_transferred);

            // Apply the next action
            bytes_transferred = 0;
            if (act.is_done())
            {
                ec = act.error();
                return;
            }
            else if (act.type() == next_action_type::read)
            {
                bytes_transferred = stream_.read_some(
                    to_buffer(act.read_args().buffer),
                    act.read_args().use_ssl,
                    io_ec
                );
            }
            else if (act.type() == next_action_type::write)
            {
                bytes_transferred = stream_.write_some(
                    asio::buffer(act.write_args().buffer),
                    act.write_args().use_ssl,
                    io_ec
                );
            }
            else if (act.type() == next_action_type::ssl_handshake)
            {
                stream_.ssl_handshake(io_ec);
            }
            else if (act.type() == next_action_type::ssl_shutdown)
            {
                stream_.ssl_shutdown(io_ec);
            }
            else if (act.type() == next_action_type::connect)
            {
                stream_.connect(act.connect_endpoint(), io_ec);
            }
            else
            {
                BOOST_ASSERT(act.type() == next_action_type::close);
                stream_.close(io_ec);
            }
        }
    }

    void async_run(any_resumable_ref resumable, asio::any_completion_handler<void(error_code)> h)
        override final
    {
        return asio::async_compose<asio::any_completion_handler<void(error_code)>, void(error_code)>(
            run_algo_op<EngineStream>(stream_, resumable),
            h,
            stream_
        );
    }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
