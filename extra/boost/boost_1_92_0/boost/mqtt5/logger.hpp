//
// Copyright (c) 2023-2026 Ivica Siladic, Bruno Iljazovic, Korina Simicevic
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MQTT5_LOGGER_HPP
#define BOOST_MQTT5_LOGGER_HPP

#include <boost/mqtt5/logger_traits.hpp>
#include <boost/mqtt5/property_types.hpp>
#include <boost/mqtt5/reason_codes.hpp>
#include <boost/mqtt5/types.hpp>

#include <boost/mqtt5/detail/traits.hpp>

#include <boost/asio/ip/tcp.hpp>
#include <boost/system/error_code.hpp>
#include <boost/type_traits/remove_cv_ref.hpp>

#include <cstdint>
#include <iostream>
#include <string_view>

namespace boost::mqtt5 {

namespace asio = boost::asio;
using error_code = boost::system::error_code;

/**
 * \brief Represents the severity level of log messages.
 */
enum class log_level : uint8_t {
    /** Error messages that indicate serious issues. **/
    error = 1,

    /** Warnings that indicate potential problems or non-critical issues. **/
    warning,

    /** Informational messages that highlight normal application behaviour and events. **/
    info,

    /** Detailed messages useful for diagnosing issues. **/
    debug
};

/**
 * \brief A logger class that can used by the \ref mqtt_client to output
 * the results of operations to stderr.
 *
 * \details All functions are invoked directly within the \ref mqtt_client using
 * its default executor. If the \ref mqtt_client is initialized with an explicit or
 * implicit strand, none of the functions will be invoked concurrently.
 * 
 * \par Thread safety
 * Distinct objects: safe. \n
 * Shared objects: unsafe. \n
 * This class is <b>not thread-safe</b>.
*/
class logger {
    constexpr static auto prefix = "[Boost.MQTT5]";

    log_level _level;
public:

    /**
     * \brief Constructs a logger that filters log messages based on the specified log level.
     *
     * \param level Messages with a log level higher than the given log level will be suppressed.
     */
    logger(log_level level = log_level::info) : _level(level) {}

    /**
     * \brief Outputs the results of the resolve operation.
     *
     * \param ec Error code returned by the resolve operation.
     * \param host Hostname used in the resolve operation.
     * \param port Port used in the resolve operation.
     * \param eps Endpoints returned by the resolve operation.
     */
    void at_resolve(
        error_code ec, std::string_view host, std::string_view port,
        const asio::ip::tcp::resolver::results_type& eps
    ) {
        if (!ec && _level < log_level::info)
            return;

        output_prefix();
        std::clog
            << "resolve: "
            << host << ":" << port;
        std::clog << " - " << ec.message() << ".";

        if (_level == log_level::debug) {
            std::clog << " [";
            for (auto it = eps.begin(); it != eps.end();) {
                std::clog << it->endpoint().address().to_string();
                if (++it != eps.end())
                    std::clog << ",";
            }
            std::clog << "]";
        }
        std::clog << std::endl;
    }

    /**
     * \brief Outputs the results of the TCP connect operation.
     *
     * \param ec Error code returned by the TCP connect operation.
     * \param ep The TCP endpoint used to establish the TCP connection.
     */
    void at_tcp_connect(error_code ec, asio::ip::tcp::endpoint ep) {
        if (!ec && _level < log_level::info)
            return;

        output_prefix();
        std::clog
            << "TCP connect: "
            << ep.address().to_string() << ":" << ep.port()
            << " - " << ec.message() << "."
        << std::endl;
    }

    /**
     * \brief Outputs the results of the TLS handshake operation.
     *
     * \param ec Error code returned by the TLS handshake operation.
     * \param ep The TCP endpoint used to establish the TLS handshake.
     */
    void at_tls_handshake(error_code ec, asio::ip::tcp::endpoint ep) {
        if (!ec && _level < log_level::info)
            return;

        output_prefix();
        std::clog
            << "TLS handshake: "
            << ep.address().to_string() << ":" << ep.port()
            << " - " << ec.message() << "."
        << std::endl;
    }

    /**
     * \brief Outputs the results of the WebSocket handshake operation.
     *
     * \param ec Error code returned by the WebSocket handshake operation.
     * \param ep The TCP endpoint used to establish the WebSocket handshake.
     */
    void at_ws_handshake(error_code ec, asio::ip::tcp::endpoint ep) {
        if (!ec && _level < log_level::info)
            return;

        output_prefix();
        std::clog
            << "WebSocket handshake: "
            << ep.address().to_string() << ":" << ep.port()
            << " - " << ec.message() << "."
        << std::endl;
    }

    /** 
     * \brief Outputs the contents of the \__CONNACK\__ packet sent by the Broker.
     * 
     * \param rc Reason Code in the received \__CONNACK\__ packet indicating
     * the result of the MQTT handshake.
     * \param session_present A flag indicating whether the Broker already has a session associated
     * with this connection.
     * \param ca_props \__CONNACK_PROPS\__ received in the \__CONNACK\__ packet.
     */
    void at_connack(
        reason_code rc,
        bool session_present, const connack_props& ca_props
    ) {
        if (!rc && _level < log_level::info)
            return;

        output_prefix();
        std::clog << "connack: " << rc.message() << ".";
        if (_level == log_level::debug) {
            std::clog << " session_present:" << session_present << " ";
            output_props(ca_props);
        }
        std::clog << std::endl;
    }

    /**
     * \brief Outputs the contents of the \__DISCONNECT\__ packet sent by the Broker.
     *
     * \param rc Reason Code in the received \__DISCONNECT\__ packet indicating
     * the reason behind the disconnection.
     * \param dc_props \__DISCONNECT_PROPS\__ received in the \__DISCONNECT\__ packet.
     */
    void at_disconnect(reason_code rc, const disconnect_props& dc_props) {
        output_prefix();
        std::clog << "disconnect: " << rc.message() << ".";
        if (_level == log_level::debug)
            output_props(dc_props);
        std::clog << std::endl;
    }

    /**
     * \brief Outputs the error message when it occurs at the transport layer
     * read or write operations.
     *
     * \param ec The error code returned by the `async_read_some` or
     * `async_write_some` operation on the underlying \__StreamType\__ stream.
     */
    void at_transport_error(error_code ec) {
        if (_level < log_level::info)
            return;

        output_prefix();
        std::clog
            << "transport layer error: " << ec.message() << "." << std::endl;
    }

private:
    void output_prefix() {
        std::clog << prefix << " ";
    }

    template <typename Props>
    void output_props(const Props& props) {
        props.visit(
            [](const auto& prop, const auto& val) -> bool {
                if constexpr (detail::is_optional<decltype(val)>) {
                    if (val.has_value()) {
                        std::clog << property_name(prop) << ":";
                        using value_type = boost::remove_cv_ref_t<decltype(*val)>;
                        if constexpr (std::is_same_v<value_type, uint8_t>)
                            std::clog << std::to_string(*val) << " ";
                        else
                            std::clog << *val << " ";
                    }
                } else { // is vector
                    if (val.empty())
                        return true;

                    std::clog << property_name(prop) << ":";
                    std::clog << "[";
                    for (size_t i = 0; i < val.size(); i++) {
                        if constexpr (detail::is_pair<decltype(val[i])>)
                            std::clog << "(" << val[i].first << "," << val[i].second << ")";
                        else
                            std::clog << std::to_string(val[i]);
                        if (i + 1 < val.size())
                            std::clog << ", ";
                    }
                    std::clog << "] ";
                }
                return true;
            }
        );
    }

    template <prop::property_type p>
    static std::string_view property_name(std::integral_constant<prop::property_type, p>) {
        return prop::name_v<p>;
    }

};

// Verify that the logger class satisfies the LoggerType concept
static_assert(has_at_resolve<logger>);
static_assert(has_at_tcp_connect<logger>);
static_assert(has_at_tls_handshake<logger>);
static_assert(has_at_ws_handshake<logger>);
static_assert(has_at_connack<logger>);
static_assert(has_at_disconnect<logger>);
static_assert(has_at_transport_error<logger>);

} // end namespace boost::mqtt5


#endif // !BOOST_MQTT5_LOGGER_HPP
