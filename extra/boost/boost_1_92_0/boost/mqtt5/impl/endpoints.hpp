//
// Copyright (c) 2023-2026 Ivica Siladic, Bruno Iljazovic, Korina Simicevic
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MQTT5_ENDPOINTS_HPP
#define BOOST_MQTT5_ENDPOINTS_HPP

#include <boost/mqtt5/detail/internal_types.hpp>
#include <boost/mqtt5/detail/log_invoke.hpp>
#include <boost/mqtt5/detail/internal_types.hpp>

#include <boost/asio/append.hpp>
#include <boost/asio/associated_allocator.hpp>
#include <boost/asio/associated_cancellation_slot.hpp>
#include <boost/asio/associated_executor.hpp>
#include <boost/asio/async_result.hpp>
#include <boost/asio/deferred.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/experimental/parallel_group.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/prepend.hpp>

#include <array>
#include <chrono>
#include <string>

namespace boost::mqtt5::detail {

namespace asio = boost::asio;

using epoints = asio::ip::tcp::resolver::results_type;

template <typename Owner, typename Handler>
class resolve_op {
    struct on_resolve {};

    Owner& _owner;

    using handler_type = Handler;
    handler_type _handler;

public:
    resolve_op(Owner& owner, Handler&& handler) :
        _owner(owner), _handler(std::move(handler))
    {}

    resolve_op(resolve_op&&) = default;
    resolve_op(const resolve_op&) = delete;

    resolve_op& operator=(resolve_op&&) = default;
    resolve_op& operator=(const resolve_op&) = delete;

    using allocator_type = asio::associated_allocator_t<handler_type>;
    allocator_type get_allocator() const noexcept {
        return asio::get_associated_allocator(_handler);
    }

    using cancellation_slot_type =
        asio::associated_cancellation_slot_t<handler_type>;
    cancellation_slot_type get_cancellation_slot() const noexcept {
        return asio::get_associated_cancellation_slot(_handler);
    }

    using executor_type = asio::associated_executor_t<handler_type>;
    executor_type get_executor() const noexcept {
        return asio::get_associated_executor(_handler);
    }

    void perform() {
        namespace asioex = boost::asio::experimental;

        if (_owner._servers.empty())
            return complete_post(asio::error::host_not_found, {}, {});

        _owner._current_host++;

        if (_owner._current_host + 1 > static_cast<int>(_owner._servers.size())) {
            _owner._current_host = -1;
            return complete_post(asio::error::try_again, {}, {});
        }

        authority_path ap = _owner._servers[_owner._current_host];

        _owner._connect_timer.expires_after(std::chrono::seconds(5));

        auto timed_resolve = asioex::make_parallel_group(
            _owner._resolver.async_resolve(ap.host, ap.port, asio::deferred),
            _owner._connect_timer.async_wait(asio::deferred)
        );

        timed_resolve.async_wait(
            asioex::wait_for_one(),
            asio::append(
                asio::prepend(std::move(*this), on_resolve {}),
                std::move(ap)
            )
        );
    }

    void operator()(
        on_resolve, std::array<std::size_t, 2> ord,
        error_code resolve_ec, epoints epts,
        error_code timer_ec, authority_path ap
    ) {
        if (
            (ord[0] == 0 && resolve_ec == asio::error::operation_aborted) ||
            (ord[0] == 1 && timer_ec == asio::error::operation_aborted)
        )
            return complete(asio::error::operation_aborted, {}, {});

        resolve_ec = timer_ec ? resolve_ec : asio::error::timed_out;
        _owner._log.at_resolve(resolve_ec, ap.host, ap.port, epts);
        if (!resolve_ec)
            return complete(error_code {}, std::move(epts), std::move(ap));

        perform();
    }

private:
    void complete(error_code ec, epoints eps, authority_path ap) {
        std::move(_handler)(ec, std::move(eps), std::move(ap));
    }

    void complete_post(error_code ec, epoints eps, authority_path ap) {
        asio::post(
            _owner.get_executor(),
            asio::prepend(
                std::move(_handler), ec,
                std::move(eps), std::move(ap)
            )
        );

    }
};


template <typename LoggerType>
class endpoints {
    using logger_type = LoggerType;

    resolver_type _resolver;
    timer_type& _connect_timer;

    std::vector<authority_path> _servers;

    int _current_host { -1 };

    log_invoke<logger_type>& _log;

    template <typename Owner, typename Handler>
    friend class resolve_op;

public:
    template <typename Executor>
    endpoints(
        Executor ex, timer_type& timer,
        log_invoke<logger_type>& log
    ) :
        _resolver(std::move(ex)), _connect_timer(timer),
        _log(log)
    {}

    endpoints(const endpoints&) = delete;
    endpoints& operator=(const endpoints&) = delete;

    void clone_servers(const endpoints& other) {
        _servers = other._servers;
    }

    using executor_type = asio::ip::tcp::resolver::executor_type;
    // NOTE: asio::ip::basic_resolver returns executor by value
    executor_type get_executor() noexcept {
        return _resolver.get_executor();
    }

    template <typename CompletionToken>
    decltype(auto) async_next_endpoint(CompletionToken&& token) {
        using Signature = void (error_code, epoints, authority_path);

        auto initiation = [](auto handler, endpoints& self) {
            resolve_op { self, std::move(handler) }.perform();
        };

        return asio::async_initiate<CompletionToken, Signature>(
            initiation, token, std::ref(*this)
        );
    }

    void brokers(std::string hosts, uint16_t default_port) {
        _servers.clear();

        // loosely based on RFC 3986
        for (auto it = hosts.cbegin(), end = hosts.cend(); it != end;) {
            skip_spaces(it, end);

            auto host = std::string(match_while("A-Za-z0-9._~-", it, end));
            if (host.empty()) break;

            std::string port;
            if (it < end && *it == ':') {
                port = std::string(match_while("0-9", ++it, end));
                if (port.empty()) break;
            }

            std::string path;
            if (it < end && *it == '/')
                path = std::string(match_while("/A-Za-z0-9._~-", it, end));

            _servers.push_back({
                std::move(host),
                port.empty()
                    ? std::to_string(default_port)
                    : std::move(port),
                std::move(path)
            });

            skip_spaces(it, end);
            if (it == end || *it++ != ',') break;
        }
    }

private:
    static constexpr void skip_spaces(
        byte_citer& it, const byte_citer& end
    ) {
        match_while(" \t\r\n\f\v", it, end);
    };


    static constexpr std::string_view match_while(
        const char* chs, byte_citer& it, const byte_citer& end
    ) {
        if (it == end)
            return {};

        auto beg = it;
        while (it < end && is_any_of(chs, *it)) ++it;

        return { &*beg, static_cast<size_t>(it - beg) };
    }

    static constexpr bool is_any_of(const char* chs, char c) {
        while (*chs)
            if (*(chs + 1) == '-' && *(chs + 2)) {
                if (c >= *chs && c <= *(chs + 2))
                    return true;
                chs += 3;
            }
            else {
                if (c == *chs)
                    return true;
                ++chs;
            }
        return false;
    }
};


} // end namespace boost::mqtt5::detail

#endif // !BOOST_MQTT5_ENDPOINTS_HPP
