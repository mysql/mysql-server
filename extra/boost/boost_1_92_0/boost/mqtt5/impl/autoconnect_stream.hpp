//
// Copyright (c) 2023-2026 Ivica Siladic, Bruno Iljazovic, Korina Simicevic
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MQTT5_AUTOCONNECT_STREAM_HPP
#define BOOST_MQTT5_AUTOCONNECT_STREAM_HPP

#include <boost/mqtt5/detail/async_mutex.hpp>
#include <boost/mqtt5/detail/async_traits.hpp>
#include <boost/mqtt5/detail/log_invoke.hpp>

#include <boost/mqtt5/impl/endpoints.hpp>
#include <boost/mqtt5/impl/read_op.hpp>
#include <boost/mqtt5/impl/reconnect_op.hpp>
#include <boost/mqtt5/impl/shutdown_op.hpp>
#include <boost/mqtt5/impl/write_op.hpp>

#include <boost/asio/async_result.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/system/error_code.hpp>

#include <cstdint>
#include <memory>
#include <string>
#include <variant> // std::monostate

namespace boost::mqtt5::detail {

namespace asio = boost::asio;
using error_code = boost::system::error_code;

template <
    typename StreamType,
    typename StreamContext = std::monostate,
    typename LoggerType = noop_logger
>
class autoconnect_stream {
public:
    using self_type = autoconnect_stream;
    using stream_type = StreamType;
    using stream_context_type = StreamContext;
    using logger_type = LoggerType;
    using executor_type = typename stream_type::executor_type;
private:
    using stream_ptr = std::shared_ptr<stream_type>;

    executor_type _stream_executor;
    async_mutex _conn_mtx;
    timer_type _read_timer, _connect_timer;
    endpoints<logger_type> _endpoints;

    stream_ptr _stream_ptr;
    stream_context_type& _stream_context;

    log_invoke<logger_type>& _log;

    template <typename Owner, typename Handler>
    friend class read_op;

    template <typename Owner, typename Handler>
    friend class write_op;

    template <typename Owner>
    friend class reconnect_op;

    template <typename Owner>
    friend class shutdown_op;

public:
    autoconnect_stream(
        const executor_type& ex, stream_context_type& context,
        log_invoke<logger_type>& log
    ) :
        _stream_executor(ex),
        _conn_mtx(_stream_executor),
        _read_timer(_stream_executor), _connect_timer(_stream_executor),
        _endpoints(_stream_executor, _connect_timer, log),
        _stream_context(context),
        _log(log)
    {
        replace_next_layer(construct_next_layer());
    }

    autoconnect_stream(const autoconnect_stream&) = delete;
    autoconnect_stream& operator=(const autoconnect_stream&) = delete;

    using next_layer_type = stream_type;
    next_layer_type& next_layer() {
        return *_stream_ptr;
    }

    const next_layer_type& next_layer() const {
        return *_stream_ptr;
    }

    executor_type get_executor() const noexcept {
        return _stream_executor;
    }

    void brokers(std::string hosts, uint16_t default_port) {
        _endpoints.brokers(std::move(hosts), default_port);
    }

    void clone_endpoints(const autoconnect_stream& other) {
        _endpoints.clone_servers(other._endpoints);
    }

    bool is_open() const noexcept {
        return lowest_layer(*_stream_ptr).is_open();
    }

    void open() {
        open_lowest_layer(_stream_ptr, asio::ip::tcp::v4());
    }

    void cancel() {
        _conn_mtx.cancel();
        _connect_timer.cancel();
    }

    void close() {
        error_code ec;
        lowest_layer(*_stream_ptr).close(ec);
    }

    template <typename CompletionToken>
    void async_shutdown(CompletionToken&& token) {
        using Signature = void (error_code);

        auto initiation = [](auto handler, self_type& self) {
            shutdown_op { self, std::move(handler) }.perform();
        };

        return asio::async_initiate<CompletionToken, Signature>(
            initiation, token, std::ref(*this)
        );
    }

    bool was_connected() const {
        error_code ec;
        lowest_layer(*_stream_ptr).remote_endpoint(ec);
        return ec == boost::system::errc::success;
    }

    template <typename BufferType, typename CompletionToken>
    decltype(auto) async_read_some(
        const BufferType& buffer, duration wait_for, CompletionToken&& token
    ) {
        using Signature = void (error_code, size_t);

        auto initiation = [](
            auto handler, self_type& self,
            const BufferType& buffer, duration wait_for
        ) {
            read_op { self, std::move(handler) }.perform(buffer, wait_for);
        };

        return asio::async_initiate<CompletionToken, Signature>(
            initiation, token, std::ref(*this), buffer, wait_for
        );
    }

    template <typename BufferType, typename CompletionToken>
    decltype(auto) async_write(
        const BufferType& buffer, CompletionToken&& token
    ) {
        using Signature = void (error_code, size_t);

        auto initiation = [](
            auto handler, self_type& self, const BufferType& buffer
        ) {
            write_op { self, std::move(handler) }.perform(buffer);
        };

        return asio::async_initiate<CompletionToken, Signature>(
            initiation, token, std::ref(*this), buffer
        );
    }

private:

    log_invoke<logger_type>& log() {
        return _log;
    }

    static void open_lowest_layer(const stream_ptr& sptr, asio::ip::tcp protocol) {
        error_code ec;
        auto& layer = lowest_layer(*sptr);
        layer.open(protocol, ec);
        layer.set_option(asio::socket_base::reuse_address(true), ec);
        layer.set_option(asio::ip::tcp::no_delay(true), ec);
    }

    stream_ptr construct_next_layer() const {
        stream_ptr sptr;
        if constexpr (has_tls_context<StreamContext>)
            sptr = std::make_shared<stream_type>(
                _stream_executor, _stream_context.tls_context()
            );
        else
            sptr = std::make_shared<stream_type>(_stream_executor);

        return sptr;
    }

    stream_ptr construct_and_open_next_layer(asio::ip::tcp protocol) const {
        auto sptr = construct_next_layer();
        open_lowest_layer(sptr, protocol);
        return sptr;
    }

    void replace_next_layer(stream_ptr sptr) {
        // close() will cancel all outstanding async operations on
        // _stream_ptr; cancelling posts operation_aborted to handlers
        // but handlers will be executed after std::exchange below;
        // handlers should therefore treat (operation_aborted && is_open())
        // equivalent to try_again.

        if (_stream_ptr)
            close();
        std::exchange(_stream_ptr, std::move(sptr));
    }

    template <typename CompletionToken>
    decltype(auto) async_reconnect(stream_ptr s, CompletionToken&& token) {
        using Signature = void (error_code);

        auto initiation = [](auto handler, self_type& self, stream_ptr s) {
            reconnect_op { self, std::move(handler) }.perform(std::move(s));
        };

        return asio::async_initiate<CompletionToken, Signature>(
            initiation, token, std::ref(*this), std::move(s)
        );
    }
};


} // end namespace boost::mqtt5::detail

#endif // !BOOST_MQTT5_AUTOCONNECT_STREAM_HPP
