//
// Copyright (c) 2023-2026 Ivica Siladic, Bruno Iljazovic, Korina Simicevic
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MQTT5_ERROR_HPP
#define BOOST_MQTT5_ERROR_HPP

#include <boost/asio/error.hpp>

#include <cstdint>
#include <ostream>
#include <string>

namespace boost::mqtt5 {

/**
 * \brief A representation of Disconnect Reason Code.
 *
 * \details Represents all Reason Codes that the Client can send to the Server
 * in the \__DISCONNECT\__ packet as the reason for the disconnection.
 */
enum class disconnect_rc_e : uint8_t {
    /** \brief Close the connection normally. Do not send the Will Message. */
    normal_disconnection = 0x00,

    /** \brief The Client wishes to disconnect but requires that
     the Server also publishes its Will Message. */
    disconnect_with_will_message = 0x04
};

namespace detail {

enum class disconnect_rc_e : uint8_t {
    normal_disconnection = 0x00,
    disconnect_with_will_message = 0x04,

    unspecified_error = 0x80,
    malformed_packet = 0x81,
    protocol_error = 0x82,
    implementation_specific_error = 0x83,
    topic_name_invalid = 0x90,
    receive_maximum_exceeded = 0x93,
    topic_alias_invalid = 0x94,
    packet_too_large = 0x95,
    message_rate_too_high = 0x96,
    quota_exceeded = 0x97,
    administrative_action = 0x98,
    payload_format_invalid = 0x99
};

}


namespace client {
/**
 * \brief Defines error codes related to MQTT client.
 *
 * \details Encapsulates errors that occur on the client side.
 */
enum class error : int {
    /** \brief The packet is malformed */
    malformed_packet = 100,

    /** \brief The packet has exceeded the Maximum Packet Size the Server is willing to accept */
    packet_too_large,

    /** \brief The Client's session does not exist or it has expired */
    session_expired,

    /** \brief There are no more available Packet Identifiers to use */
    pid_overrun,

    /** \brief The Topic is invalid and does not conform to the specification */
    invalid_topic,

    // publish
    /** \brief The Server does not support the specified \ref qos_e */
    qos_not_supported,

    /** \brief The Server does not support retained messages */
    retain_not_available,

    /** \brief The Client attempted to send a Topic Alias that is greater than Topic Alias Maximum */
    topic_alias_maximum_reached,

    // subscribe
    /** \brief The Server does not support Wildcard Subscriptions */
    wildcard_subscription_not_available,

    /** \brief The Server does not support this Subscription Identifier */
    subscription_identifier_not_available,

    /** \brief The Server does not support Shared Subscriptions */
    shared_subscription_not_available,

    // run
    /** \brief The Client is already running because \ref mqtt_client::async_run
        was already called */
    already_running,
};


inline std::string client_error_to_string(error err) {
    switch (err) {
        case error::malformed_packet:
            return "The packet is malformed";
        case error::packet_too_large:
            return "The packet has exceeded the Maximum Packet Size "
                "the Server is willing to accept";
        case error::session_expired:
            return "The Client's session does not exist or it has expired";
        case error::pid_overrun:
            return "There are no more available Packet Identifiers to use";
        case error::invalid_topic:
            return "The Topic is invalid and "
                "does not conform to the specification";
        case error::qos_not_supported:
            return "The Server does not support the specified QoS";
        case error::retain_not_available:
            return "The Server does not support retained messages";
        case error::topic_alias_maximum_reached:
            return "The Client attempted to send a Topic Alias "
                "that is greater than Topic Alias Maximum";
        case error::wildcard_subscription_not_available:
            return "The Server does not support Wildcard Subscriptions";
        case error::subscription_identifier_not_available:
            return "The Server does not support this Subscription Identifier";
        case error::shared_subscription_not_available:
            return "The Server does not support Shared Subscriptions";
        case error::already_running:
            return "The Client is already running";
        default:
            return "Unknown client error";
    }
}

struct client_ec_category : public boost::system::error_category {
    const char* name() const noexcept override { return "mqtt_client_error"; }
    std::string message(int ev) const noexcept override {
        return client_error_to_string(static_cast<error>(ev));
    }
};

/// Returns the error category associated with \ref client::error.
inline const client_ec_category& get_error_code_category() {
    static client_ec_category cat;
    return cat;
}

/// Creates an \ref error_code from a \ref client::error.
inline boost::system::error_code make_error_code(error r) {
    return { static_cast<int>(r), get_error_code_category() };
}

inline std::ostream& operator<<(std::ostream& os, const error& err) {
    os << get_error_code_category().name() << ":" << static_cast<int>(err);
    return os;
}

} // end namespace client

} // end namespace boost::mqtt5

namespace boost::system {

template <>
struct is_error_code_enum <boost::mqtt5::client::error> : std::true_type {};

} // end namespace boost::system


#endif // !BOOST_MQTT5_ERROR_HPP
