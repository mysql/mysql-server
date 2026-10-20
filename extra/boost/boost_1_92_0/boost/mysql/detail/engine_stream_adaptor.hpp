//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_ENGINE_STREAM_ADAPTOR_HPP
#define BOOST_MYSQL_DETAIL_ENGINE_STREAM_ADAPTOR_HPP

#include <boost/mysql/error_code.hpp>

#include <boost/mysql/detail/config.hpp>
#include <boost/mysql/detail/engine_impl.hpp>
#include <boost/mysql/detail/socket_stream.hpp>
#include <boost/mysql/detail/void_t.hpp>

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/config.hpp>
#include <boost/core/ignore_unused.hpp>

#include <type_traits>

// Adapts a regular Asio Stream to meet the EngineStream requirements
// We only use callbacks with the async functions in this file, so no need to support arbitrary return types

namespace boost {
namespace mysql {
namespace detail {

// Connect and close helpers
// LCOV_EXCL_START
template <class Stream>
void do_connect_impl(Stream&, const void*, error_code&, std::false_type)
{
    BOOST_ASSERT(false);
}
// LCOV_EXCL_STOP

template <class Stream>
void do_connect_impl(Stream& stream, const void* ep, error_code& ec, std::true_type)
{
    stream.lowest_layer().connect(
        *static_cast<const typename Stream::lowest_layer_type::endpoint_type*>(ep),
        ec
    );
}

template <class Stream>
void do_connect(Stream& stream, const void* ep, error_code& ec)
{
    do_connect_impl(stream, ep, ec, is_socket_stream<Stream>{});
}

// LCOV_EXCL_START
template <class Stream, class CompletionToken>
void do_async_connect_impl(Stream&, const void*, CompletionToken&&, std::false_type)
{
    BOOST_ASSERT(false);
}
// LCOV_EXCL_STOP

template <class Stream, class CompletionToken>
void do_async_connect_impl(Stream& stream, const void* ep, CompletionToken&& token, std::true_type)
{
    stream.lowest_layer().async_connect(
        *static_cast<const typename Stream::lowest_layer_type::endpoint_type*>(ep),
        std::forward<CompletionToken>(token)
    );
}

template <class Stream, class CompletionToken>
void do_async_connect(Stream& stream, const void* ep, CompletionToken&& token)
{
    do_async_connect_impl(stream, ep, std::forward<CompletionToken>(token), is_socket_stream<Stream>{});
}

// LCOV_EXCL_START
template <class Stream>
void do_close_impl(Stream&, error_code&, std::false_type)
{
    BOOST_ASSERT(false);
}
// LCOV_EXCL_STOP

template <class Stream>
void do_close_impl(Stream& stream, error_code& ec, std::true_type)
{
    stream.lowest_layer().shutdown(asio::socket_base::shutdown_both, ec);
    stream.lowest_layer().close(ec);
}

template <class Stream>
void do_close(Stream& stream, error_code& ec)
{
    do_close_impl(stream, ec, is_socket_stream<Stream>{});
}

template <class Stream>
class engine_stream_adaptor
{
    Stream stream_;

public:
    template <class... Args>
    engine_stream_adaptor(Args&&... args) : stream_(std::forward<Args>(args)...)
    {
    }

    Stream& stream() { return stream_; }
    const Stream& stream() const { return stream_; }

    bool supports_ssl() const { return false; }

    using executor_type = asio::any_io_executor;
    executor_type get_executor() { return stream_.get_executor(); }

    // SSL
    // LCOV_EXCL_START
    void ssl_handshake(error_code&) { BOOST_ASSERT(false); }

    template <class CompletinToken>
    void async_ssl_handshake(CompletinToken&&)
    {
        BOOST_ASSERT(false);
    }

    void ssl_shutdown(error_code&) { BOOST_ASSERT(false); }

    template <class CompletionToken>
    void async_ssl_shutdown(CompletionToken&&)
    {
        BOOST_ASSERT(false);
    }
    // LCOV_EXCL_STOP

    // Reading
    std::size_t read_some(boost::asio::mutable_buffer buff, bool use_ssl, error_code& ec)
    {
        BOOST_ASSERT(!use_ssl);
        boost::ignore_unused(use_ssl);
        return stream_.read_some(buff, ec);
    }

    template <class CompletionToken>
    void async_read_some(boost::asio::mutable_buffer buff, bool use_ssl, CompletionToken&& token)
    {
        BOOST_ASSERT(!use_ssl);
        boost::ignore_unused(use_ssl);
        stream_.async_read_some(buff, std::forward<CompletionToken>(token));
    }

    // Writing
    std::size_t write_some(boost::asio::const_buffer buff, bool use_ssl, error_code& ec)
    {
        BOOST_ASSERT(!use_ssl);
        boost::ignore_unused(use_ssl);
        return stream_.write_some(buff, ec);
    }

    template <class CompletionToken>
    void async_write_some(boost::asio::const_buffer buff, bool use_ssl, CompletionToken&& token)
    {
        BOOST_ASSERT(!use_ssl);
        boost::ignore_unused(use_ssl);
        stream_.async_write_some(buff, std::forward<CompletionToken>(token));
    }

    // Connect and close
    void connect(const void* endpoint, error_code& ec) { do_connect(stream_, endpoint, ec); }

    template <class CompletionToken>
    void async_connect(const void* endpoint, CompletionToken&& token)
    {
        do_async_connect(stream_, endpoint, std::forward<CompletionToken>(token));
    }

    void close(error_code& ec) { do_close(stream_, ec); }
};

template <class Stream>
class engine_stream_adaptor<asio::ssl::stream<Stream>>
{
    asio::ssl::stream<Stream> stream_;

public:
    template <class... Args>
    engine_stream_adaptor(Args&&... args) : stream_(std::forward<Args>(args)...)
    {
    }

    asio::ssl::stream<Stream>& stream() { return stream_; }
    const asio::ssl::stream<Stream>& stream() const { return stream_; }

    bool supports_ssl() const { return true; }

    using executor_type = asio::any_io_executor;
    executor_type get_executor() { return stream_.get_executor(); }

    // SSL
    void ssl_handshake(error_code& ec) { stream_.handshake(asio::ssl::stream_base::client, ec); }

    template <class CompletionToken>
    void async_ssl_handshake(CompletionToken&& token)
    {
        stream_.async_handshake(asio::ssl::stream_base::client, std::forward<CompletionToken>(token));
    }

    void ssl_shutdown(error_code& ec) { stream_.shutdown(ec); }

    template <class CompletionToken>
    void async_ssl_shutdown(CompletionToken&& token)
    {
        stream_.async_shutdown(std::forward<CompletionToken>(token));
    }

    // Reading
    std::size_t read_some(boost::asio::mutable_buffer buff, bool use_ssl, error_code& ec)
    {
        if (use_ssl)
        {
            return stream_.read_some(buff, ec);
        }
        else
        {
            return stream_.next_layer().read_some(buff, ec);
        }
    }

    template <class CompletionToken>
    void async_read_some(boost::asio::mutable_buffer buff, bool use_ssl, CompletionToken&& token)
    {
        if (use_ssl)
        {
            stream_.async_read_some(buff, std::forward<CompletionToken>(token));
        }
        else
        {
            stream_.next_layer().async_read_some(buff, std::forward<CompletionToken>(token));
        }
    }

    // Writing
    std::size_t write_some(boost::asio::const_buffer buff, bool use_ssl, error_code& ec)
    {
        if (use_ssl)
        {
            return stream_.write_some(buff, ec);
        }
        else
        {
            return stream_.next_layer().write_some(buff, ec);
        }
    }

    template <class CompletionToken>
    void async_write_some(boost::asio::const_buffer buff, bool use_ssl, CompletionToken&& token)
    {
        if (use_ssl)
        {
            stream_.async_write_some(buff, std::forward<CompletionToken>(token));
        }
        else
        {
            stream_.next_layer().async_write_some(buff, std::forward<CompletionToken>(token));
        }
    }

    // Connect and close
    void connect(const void* endpoint, error_code& ec) { do_connect(stream_, endpoint, ec); }

    template <class CompletionToken>
    void async_connect(const void* endpoint, CompletionToken&& token)
    {
        do_async_connect(stream_, endpoint, std::forward<CompletionToken>(token));
    }

    void close(error_code& ec) { do_close(stream_, ec); }
};

#ifdef BOOST_MYSQL_SEPARATE_COMPILATION
extern template class engine_impl<engine_stream_adaptor<asio::ssl::stream<asio::ip::tcp::socket>>>;
extern template class engine_impl<engine_stream_adaptor<asio::ip::tcp::socket>>;
#endif

template <class Stream, class... Args>
std::unique_ptr<engine> make_engine(Args&&... args)
{
    return std::unique_ptr<engine>(new engine_impl<engine_stream_adaptor<Stream>>(std::forward<Args>(args)...)
    );
}

// Use these only for engines created using make_engine
template <class Stream>
Stream& stream_from_engine(engine& eng)
{
    using derived_t = engine_impl<engine_stream_adaptor<Stream>>;
    return static_cast<derived_t&>(eng).stream().stream();
}

template <class Stream>
const Stream& stream_from_engine(const engine& eng)
{
    using derived_t = engine_impl<engine_stream_adaptor<Stream>>;
    return static_cast<const derived_t&>(eng).stream().stream();
}

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
