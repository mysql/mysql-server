//
// Copyright (c) 2023-2026 Ivica Siladic, Bruno Iljazovic, Korina Simicevic
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MQTT5_LOG_INVOKE_HPP
#define BOOST_MQTT5_LOG_INVOKE_HPP

#include <boost/mqtt5/logger_traits.hpp>
#include <boost/mqtt5/property_types.hpp>
#include <boost/mqtt5/reason_codes.hpp>
#include <boost/mqtt5/types.hpp>

#include <boost/asio/ip/tcp.hpp>
#include <boost/system/error_code.hpp>

#include <string_view>
#include <type_traits>

namespace boost::mqtt5::detail {

namespace asio = boost::asio;
using boost::system::error_code;

template <typename LoggerType>
class log_invoke {
    LoggerType _logger;
public:
    explicit log_invoke(LoggerType logger = {}) :
        _logger(std::move(logger))
    {}

    void at_resolve(
        error_code ec, std::string_view host, std::string_view port,
        const asio::ip::tcp::resolver::results_type& eps
    ) {
        if constexpr (has_at_resolve<LoggerType>)
            _logger.at_resolve(ec, host, port, eps);
    }

    void at_tcp_connect(error_code ec, asio::ip::tcp::endpoint ep) {
        if constexpr (has_at_tcp_connect<LoggerType>)
            _logger.at_tcp_connect(ec, ep);
    }

    void at_tls_handshake(error_code ec, asio::ip::tcp::endpoint ep) {
        if constexpr (has_at_tls_handshake<LoggerType>)
            _logger.at_tls_handshake(ec, ep);
    }

    void at_ws_handshake(error_code ec, asio::ip::tcp::endpoint ep) {
        if constexpr (has_at_ws_handshake<LoggerType>)
            _logger.at_ws_handshake(ec, ep);
    }

    void at_connack(
        reason_code rc,
        bool session_present, const connack_props& ca_props
    ) {
        if constexpr (has_at_connack<LoggerType>)
            _logger.at_connack(rc, session_present, ca_props);
    }

    void at_disconnect(reason_code rc, const disconnect_props& dc_props) {
        if constexpr (has_at_disconnect<LoggerType>)
            _logger.at_disconnect(rc, dc_props);
    }

    void at_transport_error(error_code ec) {
        if constexpr (has_at_transport_error<LoggerType>)
            _logger.at_transport_error(ec);
    }
};

} // end namespace boost::mqtt5::detail


#endif // !BOOST_MQTT5_LOG_INVOKE_HPP
