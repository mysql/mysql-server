//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_INTERNAL_VARIANT_STREAM_HPP
#define BOOST_MYSQL_IMPL_INTERNAL_VARIANT_STREAM_HPP

#include <boost/mysql/any_address.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/mysql/detail/access.hpp>

#include <boost/mysql/impl/internal/coroutine.hpp>
#include <boost/mysql/impl/internal/ssl_context_with_default.hpp>

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/associated_immediate_executor.hpp>
#include <boost/asio/cancellation_type.hpp>
#include <boost/asio/compose.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/generic/stream_protocol.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/local/stream_protocol.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/core/span.hpp>
#include <boost/optional/optional.hpp>

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace boost {
namespace mysql {
namespace detail {

struct variant_stream_state
{
    asio::generic::stream_protocol::socket sock;
    ssl_context_with_default ssl_ctx;
    boost::optional<asio::ssl::stream<asio::generic::stream_protocol::socket&>> ssl;

    variant_stream_state(asio::any_io_executor ex, asio::ssl::context* ctx) : sock(ex), ssl_ctx(ctx) {}

    asio::ssl::stream<asio::generic::stream_protocol::socket&>& create_ssl_stream()
    {
        // The stream object must be re-created even if it already exists, since
        // once used for a connection (anytime after ssl::stream::handshake is called),
        // it can't be re-used for any subsequent connections
        ssl.emplace(sock, ssl_ctx.get());
        return *ssl;
    }
};

enum class vsconnect_action_type
{
    none,
    resolve,
    connect,
    immediate,  // we'll be performing an immediate completion
};

struct vsconnect_action
{
    vsconnect_action_type type;

    union data_t
    {
        error_code err;
        struct resolve_t
        {
            const std::string* hostname;
            const std::string* service;
        } resolve;
        span<const asio::generic::stream_protocol::endpoint> connect;

        data_t(error_code v) noexcept : err(v) {}
        data_t(resolve_t v) noexcept : resolve(v) {}
        data_t(span<const asio::generic::stream_protocol::endpoint> v) noexcept : connect(v) {}
    } data;

    struct immediate_tag
    {
    };

    vsconnect_action(immediate_tag) noexcept : type(vsconnect_action_type::immediate), data(error_code()) {}
    vsconnect_action(error_code v = {}) noexcept : type(vsconnect_action_type::none), data(v) {}
    vsconnect_action(data_t::resolve_t v) noexcept : type(vsconnect_action_type::resolve), data(v) {}
    vsconnect_action(span<const asio::generic::stream_protocol::endpoint> v) noexcept
        : type(vsconnect_action_type::connect), data(v)
    {
    }
};

class variant_stream_connect_algo
{
    variant_stream_state* st_;
    const any_address* addr_;
    boost::optional<asio::ip::tcp::resolver> resolv_;
    std::vector<asio::generic::stream_protocol::endpoint> endpoints_;
    std::string service_;
    int resume_point_{0};

    const std::string& address() const { return access::get_impl(*addr_).address; }
    asio::any_io_executor get_executor() const { return st_->sock.get_executor(); }

public:
    variant_stream_connect_algo(variant_stream_state& st, const any_address& addr) : st_(&st), addr_(&addr) {}

    asio::ip::tcp::resolver& resolver() { return *resolv_; }
    asio::generic::stream_protocol::socket& socket() { return st_->sock; }

    vsconnect_action resume(
        error_code ec,
        const asio::ip::tcp::resolver::results_type* resolver_results,
        asio::cancellation_type_t cancel_state
    )
    {
        // All errors are considered fatal
        if (ec)
            return ec;

        // If we received a terminal cancellation signal, exit with the appropriate error code.
        // In composed async operations, if the cancellation arrives after an intermediate operation
        // has completed, but before the handler is called, the operation finishes successfully,
        // but the cancellation state is set. This check covers this case.
        if (!!(cancel_state & asio::cancellation_type_t::terminal))
            return error_code(asio::error::operation_aborted);

        switch (resume_point_)
        {
        case 0:

            // Clean up any previous state
            st_->sock = asio::generic::stream_protocol::socket(get_executor());

            // Set up the endpoints vector
            if (addr_->type() == address_type::host_and_port)
            {
                // Emplace the resolver
                resolv_.emplace(get_executor());

                // Resolve the endpoints
                service_ = std::to_string(addr_->port());
                BOOST_MYSQL_YIELD(resume_point_, 1, vsconnect_action({&address(), &service_}));

                // Convert them to a vector of type-erased endpoints.
                // This workarounds https://github.com/chriskohlhoff/asio/issues/1502
                // and makes connect() uniform for TCP and UNIX
                endpoints_.reserve(resolver_results->size());
                for (const auto& entry : *resolver_results)
                {
                    endpoints_.push_back(entry.endpoint());
                }
            }
            else
            {
                BOOST_ASSERT(addr_->type() == address_type::unix_path);
#ifdef BOOST_ASIO_HAS_LOCAL_SOCKETS
                endpoints_.push_back(asio::local::stream_protocol::endpoint(address()));
#else
                BOOST_MYSQL_YIELD(resume_point_, 3, vsconnect_action::immediate_tag{});
                return vsconnect_action(asio::error::operation_not_supported);
#endif
            }

            // Actually connect
            BOOST_MYSQL_YIELD(resume_point_, 2, vsconnect_action{endpoints_});

            // If we're doing TCP, disable Naggle's algorithm
            if (addr_->type() == address_type::host_and_port)
            {
                st_->sock.set_option(asio::ip::tcp::no_delay(true));
            }

            // Done
        }

        return {};
    }
};

// Implements the EngineStream concept (see stream_adaptor)
class variant_stream
{
public:
    variant_stream(asio::any_io_executor ex, asio::ssl::context* ctx) : st_(std::move(ex), ctx) {}

    bool supports_ssl() const { return true; }

    // Executor
    using executor_type = asio::any_io_executor;
    executor_type get_executor() { return st_.sock.get_executor(); }

    // SSL
    void ssl_handshake(error_code& ec)
    {
        st_.create_ssl_stream().handshake(asio::ssl::stream_base::client, ec);
    }

    template <class CompletionToken>
    void async_ssl_handshake(CompletionToken&& token)
    {
        st_.create_ssl_stream();
        st_.ssl->async_handshake(asio::ssl::stream_base::client, std::forward<CompletionToken>(token));
    }

    void ssl_shutdown(error_code& ec)
    {
        BOOST_ASSERT(st_.ssl.has_value());
        st_.ssl->shutdown(ec);
    }

    template <class CompletionToken>
    void async_ssl_shutdown(CompletionToken&& token)
    {
        BOOST_ASSERT(st_.ssl.has_value());
        st_.ssl->async_shutdown(std::forward<CompletionToken>(token));
    }

    // Reading
    std::size_t read_some(asio::mutable_buffer buff, bool use_ssl, error_code& ec)
    {
        if (use_ssl)
        {
            BOOST_ASSERT(st_.ssl.has_value());
            return st_.ssl->read_some(buff, ec);
        }
        else
        {
            return st_.sock.read_some(buff, ec);
        }
    }

    template <class CompletionToken>
    void async_read_some(asio::mutable_buffer buff, bool use_ssl, CompletionToken&& token)
    {
        if (use_ssl)
        {
            BOOST_ASSERT(st_.ssl.has_value());
            st_.ssl->async_read_some(buff, std::forward<CompletionToken>(token));
        }
        else
        {
            st_.sock.async_read_some(buff, std::forward<CompletionToken>(token));
        }
    }

    // Writing
    std::size_t write_some(boost::asio::const_buffer buff, bool use_ssl, error_code& ec)
    {
        if (use_ssl)
        {
            BOOST_ASSERT(st_.ssl.has_value());
            return st_.ssl->write_some(buff, ec);
        }
        else
        {
            return st_.sock.write_some(buff, ec);
        }
    }

    template <class CompletionToken>
    void async_write_some(boost::asio::const_buffer buff, bool use_ssl, CompletionToken&& token)
    {
        if (use_ssl)
        {
            BOOST_ASSERT(st_.ssl.has_value());
            return st_.ssl->async_write_some(buff, std::forward<CompletionToken>(token));
        }
        else
        {
            return st_.sock.async_write_some(buff, std::forward<CompletionToken>(token));
        }
    }

    // Connect and close
    void connect(const void* server_address, error_code& output_ec)
    {
        // Setup
        variant_stream_connect_algo algo(st_, *static_cast<const any_address*>(server_address));
        error_code ec;
        asio::ip::tcp::resolver::results_type resolver_results;

        // Run until complete
        while (true)
        {
            // The sync algorithm doesn't support cancellation
            auto act = algo.resume(ec, &resolver_results, asio::cancellation_type_t::none);
            switch (act.type)
            {
            case vsconnect_action_type::connect: asio::connect(st_.sock, act.data.connect, ec); break;
            case vsconnect_action_type::resolve:
                resolver_results = algo.resolver()
                                       .resolve(*act.data.resolve.hostname, *act.data.resolve.service, ec);
                break;
            case vsconnect_action_type::immediate: break;  // has effect only for async
            case vsconnect_action_type::none: output_ec = act.data.err; return;
            default: BOOST_ASSERT(false);  // LCOV_EXCL_LINE
            }
        }
    }

    template <class CompletionToken>
    void async_connect(const void* server_address, CompletionToken&& token)
    {
        asio::async_compose<CompletionToken, void(error_code)>(
            connect_op(*this, *static_cast<const any_address*>(server_address)),
            token,
            get_executor()
        );
    }

    void close(error_code& ec)
    {
        st_.sock.shutdown(asio::generic::stream_protocol::socket::shutdown_both, ec);
        st_.sock.close(ec);
    }

    // Exposed for testing
    const asio::generic::stream_protocol::socket& socket() const { return st_.sock; }

private:
    variant_stream_state st_;

    struct connect_op
    {
        std::unique_ptr<variant_stream_connect_algo> algo_;

        connect_op(variant_stream& this_obj, const any_address& server_address)
            : algo_(new variant_stream_connect_algo(this_obj.st_, server_address))
        {
        }

        template <class Self>
        void operator()(
            Self& self,
            error_code ec = {},
            const asio::ip::tcp::resolver::results_type& resolver_results = {}
        )
        {
            auto act = algo_->resume(ec, &resolver_results, self.cancelled());
            switch (act.type)
            {
            case vsconnect_action_type::connect:
                asio::async_connect(algo_->socket(), act.data.connect, std::move(self));
                break;
            case vsconnect_action_type::resolve:
                algo_->resolver()
                    .async_resolve(*act.data.resolve.hostname, *act.data.resolve.service, std::move(self));
                break;
            case vsconnect_action_type::immediate:
                asio::dispatch(
                    asio::get_associated_immediate_executor(self, self.get_io_executor()),
                    std::move(self)
                );
                break;
            case vsconnect_action_type::none:
                algo_.reset();
                self.complete(act.data.err);
                break;
            default: BOOST_ASSERT(false);  // LCOV_EXCL_LINE
            }
        }

        // Signature for range connect
        template <class Self>
        void operator()(Self& self, error_code ec, asio::generic::stream_protocol::endpoint)
        {
            (*this)(self, ec, asio::ip::tcp::resolver::results_type{});
        }
    };
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
