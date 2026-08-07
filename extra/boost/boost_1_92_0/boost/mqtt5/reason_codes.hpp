//
// Copyright (c) 2023-2026 Ivica Siladic, Bruno Iljazovic, Korina Simicevic
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MQTT5_REASON_CODES_HPP
#define BOOST_MQTT5_REASON_CODES_HPP

#include <algorithm>
#include <cstdint>
#include <optional>
#include <ostream>
#include <type_traits>
#include <utility>

namespace boost::mqtt5 {

/// \cond internal
namespace reason_codes {

enum class category : uint8_t {
    none,
    connack, puback, pubrec,
    pubrel, pubcomp, suback,
    unsuback, auth, disconnect
};

} // end namespace reason_codes

/// \endcond

/**
 * \brief A class holding Reason Code values originating from Control Packets.
 *
 * \details A Reason Code is a one byte unsigned value that indicates the result of an operation.
 *    Reason Codes less than 0x80 indicate successful completion of an operation.
 *    The normal Reason Code for success is 0.
 *    Reason Code values of 0x80 or greater indicate failure.
 *    The \__CONNACK\__,  \__PUBACK\__,  \__PUBREC\__,  \__PUBREL\__,  \__PUBCOMP\__,  \__DISCONNECT\__
 *    and  \__AUTH\__ Control Packets have a single Reason Code as part of the Variable Header.
 *    The \__SUBACK\__ and \__UNSUBACK\__ packets contain a list of one or more Reason Codes in the Payload.
 *
 *    \see See \__REASON_CODES\__ for a complete list of all possible instances of this class.
 */
class reason_code {
    uint8_t _code;
    reason_codes::category _category { reason_codes::category::none };
public:
/// \cond internal
    constexpr reason_code() : _code(0xff) {}

    constexpr reason_code(uint8_t code, reason_codes::category cat) :
        _code(code), _category(cat)
    {}

    constexpr explicit reason_code(uint8_t code) : _code(code) {}
/// \endcond

    /**
     * \brief Indication if the object holds a Reason Code indicating an error.
     *
     * \details Any Reason Code holding a value equal to or greater than 0x80.
     */
    explicit operator bool() const noexcept {
        return _code >= 0x80;
    }

    /**
     * \brief Returns the byte value of the Reason Code.
     */
    constexpr uint8_t value() const noexcept {
        return _code;
    }

    /// Insertion operator.
    friend std::ostream& operator<<(std::ostream& os, const reason_code& rc) {
        os << rc.message();
        return os;
    }

    /// Operator less than.
    friend bool operator<(const reason_code& lhs, const reason_code& rhs) {
        return lhs._code < rhs._code;
    }

    /// Equality operator.
    friend bool operator==(const reason_code& lhs, const reason_code& rhs) {
        return lhs._code == rhs._code && lhs._category == rhs._category;
    }

    /**
     * \brief Returns a message describing the meaning behind the Reason Code.
     */
    std::string message() const {
        switch (_code) {
            case 0x00:
                if (_category == reason_codes::category::suback)
                    return "The subscription is accepted with maximum QoS sent at 0";
                if (_category == reason_codes::category::disconnect)
                    return "Close the connection normally. Do not send the Will Message";
                return "The operation completed successfully";
            case 0x01:
                return "The subscription is accepted with maximum QoS sent at 1";
            case 0x02:
                return "The subscription is accepted with maximum QoS sent at 2";
            case 0x04:
                return "The Client wishes to disconnect but requires"
                        "that the Server also publishes its Will Message";
            case 0x10:
                return "The message is accepted but there are no subscribers";
            case 0x11:
                return "No matching Topic Filter is being used by the Client.";
            case 0x18:
                return "Continue the authentication with another step";
            case 0x19:
                return "Initiate a re-authentication";
            case 0x80:
                return "The Server does not wish to reveal the reason for the"
                        "failure or none of the other Reason Codes apply";
            case 0x81:
                return "Data within the packet could not be correctly parsed";
            case 0x82:
                return "Data in the packet does not conform to this specification";
            case 0x83:
                return "The packet is valid but not accepted by this Server";
            case 0x84:
                return "The Server does not support the requested "
                        "version of the MQTT protocol";
            case 0x85:
                return "The Client ID is valid but not allowed by this Server";
            case 0x86:
                return "The Server does not accept the User Name or Password provided";
            case 0x87:
                return "The request is not authorized";
            case 0x88:
                return "The MQTT Server is not available";
            case 0x89:
                return "The MQTT Server is busy, try again later";
            case 0x8a:
                return "The Client has been banned by administrative action";
            case 0x8b:
                return "The Server is shutting down";
            case 0x8c:
                return "The authentication method is not supported or "
                        "does not match the method currently in use";
            case 0x8d:
                return "No packet has been received for 1.5 times the Keepalive time";
            case 0x8e:
                return "Another Connection using the same ClientID has connected "
                        "causing this Connection to be closed";
            case 0x8f:
                return "The Topic Filer is not malformed, but it is not accepted";
            case 0x90:
                return "The Topic Name is not malformed, but it is not accepted";
            case 0x91:
                return "The Packet Identifier is already in use";
            case 0x92:
                return "The Packet Identifier is not known";
            case 0x93:
                return "The Client or Server has received more than the Receive "
                        "Maximum publication for which it has not sent PUBACK or PUBCOMP";
            case 0x94:
                return "The Client or Server received a PUBLISH packet containing "
                        "a Topic Alias greater than the Maximum Topic Alias";
            case 0x95:
                return "The packet exceeded the maximum permissible size";
            case 0x96:
                return "The received data rate is too high";
            case 0x97:
                return "An implementation or administrative imposed limit has been exceeded";
            case 0x98:
                return "The Connection is closed due to an administrative action";
            case 0x99:
                return "The Payload does not match the specified Payload Format Indicator";
            case 0x9a:
                return "The Server does not support retained messages";
            case 0x9b:
                return "The Server does not support the QoS the Client specified or "
                        "it is greater than the Maximum QoS specified";
            case 0x9c:
                return "The Client should temporarily use another server";
            case 0x9d:
                return "The Client should permanently use another server";
            case 0x9e:
                return "The Server does not support Shared Subscriptions for this Client";
            case 0x9f:
                return "The connection rate limit has been exceeded";
            case 0xa0:
                return "The maximum connection time authorized for this "
                        "connection has been exceeded";
            case 0xa1:
                return "The Server does not support Subscription Identifiers";
            case 0xa2:
                return "The Server does not support Wildcard Subscriptions";
            case 0xff:
                return "No reason code";
            default:
                return "Invalid reason code";
        }
    }
};

namespace reason_codes {

/** \brief  No Reason Code. A \ref client::error occurred.*/
constexpr reason_code empty {};

/** \brief The operation completed successfully. */
constexpr reason_code success { 0x00 };

/** \brief Close the connection normally. Do not send the Will Message. */
constexpr reason_code normal_disconnection { 0x00, category::disconnect };

/** \brief The subscription is accepted with maximum QoS sent at 0. */
constexpr reason_code granted_qos_0 { 0x00, category::suback };

/** \brief The subscription is accepted with maximum QoS sent at 1. */
constexpr reason_code granted_qos_1 { 0x01 };

/** \brief The subscription is accepted with maximum QoS sent at 2 */
constexpr reason_code granted_qos_2 { 0x02 };

/** \brief The Client wishes to disconnect but requires that
 the Server also publishes its Will Message. */
constexpr reason_code disconnect_with_will_message { 0x04 };

/** \brief The message is accepted but there are no subscribers. */
constexpr reason_code no_matching_subscribers { 0x10 };

/** \brief No matching Topic Filter is being used by the Client. */
constexpr reason_code no_subscription_existed { 0x11 };

/** \brief Continue the authentication with another step. */
constexpr reason_code continue_authentication { 0x18 };

/** \brief Initiate a re-authentication. */
constexpr reason_code reauthenticate { 0x19 };

/** \brief The Server does not wish to reveal the reason for the
 failure or none of the other Reason Codes apply. */
constexpr reason_code unspecified_error { 0x80 };

/** \brief Data within the packet could not be correctly parsed. */
constexpr reason_code malformed_packet { 0x81 };

/** \brief Data in the packet does not conform to this specification. */
constexpr reason_code protocol_error { 0x82 };

/** \brief The packet is valid but not accepted by this Server. */
constexpr reason_code implementation_specific_error { 0x83 };

/** \brief The Server does not support the requested version of the MQTT protocol. */
constexpr reason_code unsupported_protocol_version { 0x84 };

/** \brief The Client ID is valid but not allowed by this Server. */
constexpr reason_code client_identifier_not_valid { 0x85 };

/** \brief The Server does not accept the User Name or Password provided. */
constexpr reason_code bad_username_or_password { 0x86 };

/** \brief The request is not authorized. */
constexpr reason_code not_authorized { 0x87 };

/** \brief The MQTT Server is not available. */
constexpr reason_code server_unavailable { 0x88 };

/** \brief The MQTT Server is busy, try again later. */
constexpr reason_code server_busy { 0x89 };

/** \brief The Client has been banned by administrative action. */
constexpr reason_code banned { 0x8a };

/** \brief The Server is shutting down. */
constexpr reason_code server_shutting_down { 0x8b };

/** \brief The authentication method is not supported or
 does not match the method currently in use. */
constexpr reason_code bad_authentication_method { 0x8c };

/** \brief No packet has been received for 1.5 times the Keepalive time. */
constexpr reason_code keep_alive_timeout { 0x8d };

/** \brief Another Connection using the same ClientID has connected
 causing this Connection to be closed. */
constexpr reason_code session_taken_over { 0x8e };

/** \brief The Topic Filter is not malformed, but it is not accepted. */
constexpr reason_code topic_filter_invalid { 0x8f };

/** \brief The Topic Name is not malformed, but it is not accepted. */
constexpr reason_code topic_name_invalid { 0x90 };

/** \brief The Packet Identifier is already in use. */
constexpr reason_code packet_identifier_in_use { 0x91 };

/** \brief The Packet Identifier is not known. */
constexpr reason_code packet_identifier_not_found { 0x92 };

/** \brief The Client or Server has received more than the Receive
 Maximum publication for which it has not sent PUBACK or PUBCOMP. */
constexpr reason_code receive_maximum_exceeded { 0x93 };

/** \brief The Client or Server received a PUBLISH packet containing
 a Topic Alias greater than the Maximum Topic Alias. */
constexpr reason_code topic_alias_invalid { 0x94 };

/** \brief The packet exceeded the maximum permissible size. */
constexpr reason_code packet_too_large { 0x95 };

/** \brief The received data rate is too high. */
constexpr reason_code message_rate_too_high { 0x96 };

/** \brief An implementation or administrative imposed limit has been exceeded. */
constexpr reason_code quota_exceeded { 0x97 };

/** \brief The Connection is closed due to an administrative action. */
constexpr reason_code administrative_action { 0x98 };

/** \brief The Payload does not match the specified Payload Format Indicator. */
constexpr reason_code payload_format_invalid { 0x99 };

/** \brief The Server does not support retained messages. */
constexpr reason_code retain_not_supported { 0x9a };

/** \brief The Server does not support the QoS the Client specified or
 it is greater than the Maximum QoS specified. */
constexpr reason_code qos_not_supported { 0x9b };

/** \brief The Client should temporarily use another server. */
constexpr reason_code use_another_server { 0x9c };

/** \brief The Client should permanently use another server. */
constexpr reason_code server_moved { 0x9d };

/** \brief The Server does not support Shared Subscriptions for this Client. */
constexpr reason_code shared_subscriptions_not_supported { 0x9e };

/** \brief The connection rate limit has been exceeded. */
constexpr reason_code connection_rate_exceeded { 0x9f };

/** \brief The maximum connection time authorized for this
 connection has been exceeded. */
constexpr reason_code maximum_connect_time { 0xa0 };

/** \brief The Server does not support Subscription Identifiers. */
constexpr reason_code subscription_ids_not_supported { 0xa1 };

/** \brief The Server does not support Wildcard Subscriptions. */
constexpr reason_code wildcard_subscriptions_not_supported { 0xa2 };

namespace detail {

template <
    category cat,
    std::enable_if_t<cat == category::connack, bool> = true
>
std::pair<reason_code*, size_t> valid_codes() {
    static reason_code valid_codes[] = {
        success, unspecified_error, malformed_packet,
        protocol_error, implementation_specific_error,
        unsupported_protocol_version, client_identifier_not_valid,
        bad_username_or_password, not_authorized,
        server_unavailable, server_busy, banned,
        bad_authentication_method, topic_name_invalid,
        packet_too_large, quota_exceeded,
        payload_format_invalid, retain_not_supported,
        qos_not_supported, use_another_server,
        server_moved, connection_rate_exceeded
    };
    static size_t len = sizeof(valid_codes) / sizeof(reason_code);
    return std::make_pair(valid_codes, len);
}

template <
    category cat,
    std::enable_if_t<cat == category::auth, bool> = true
>
std::pair<reason_code*, size_t> valid_codes() {
    static reason_code valid_codes[] = {
        success, continue_authentication, reauthenticate
    };
    static size_t len = sizeof(valid_codes) / sizeof(reason_code);
    return std::make_pair(valid_codes, len);
}

template <
    category cat,
    std::enable_if_t<
        cat == category::puback || cat == category::pubrec, bool
    > = true
>
std::pair<reason_code*, size_t> valid_codes() {
    static reason_code valid_codes[] = {
        success, no_matching_subscribers, unspecified_error,
        implementation_specific_error, not_authorized,
        topic_name_invalid, packet_identifier_in_use,
        quota_exceeded, payload_format_invalid
    };
    static size_t len = sizeof(valid_codes) / sizeof(reason_code);
    return std::make_pair(valid_codes, len);
}

template <
    category cat,
    std::enable_if_t<
        cat == category::pubrel || cat == category::pubcomp, bool
    > = true
>
std::pair<reason_code*, size_t> valid_codes() {
    static reason_code valid_codes[] = {
        success, packet_identifier_not_found
    };
    static size_t len = sizeof(valid_codes) / sizeof(reason_code);
    return std::make_pair(valid_codes, len);
}

template <
    category cat,
    std::enable_if_t<cat == category::suback, bool> = true
>
std::pair<reason_code*, size_t> valid_codes() {
    static reason_code valid_codes[] = {
        granted_qos_0, granted_qos_1, granted_qos_2,
        unspecified_error, implementation_specific_error,
        not_authorized, topic_filter_invalid,
        packet_identifier_in_use, quota_exceeded,
        shared_subscriptions_not_supported,
        subscription_ids_not_supported,
        wildcard_subscriptions_not_supported
    };
    static size_t len = sizeof(valid_codes) / sizeof(reason_code);
    return std::make_pair(valid_codes, len);
}

template <
    category cat,
    std::enable_if_t<cat == category::unsuback, bool> = true
>
std::pair<reason_code*, size_t> valid_codes() {
    static reason_code valid_codes[] = {
        success, no_subscription_existed,
        unspecified_error, implementation_specific_error,
        not_authorized, topic_filter_invalid,
        packet_identifier_in_use
    };
    static size_t len = sizeof(valid_codes) / sizeof(reason_code);
    return std::make_pair(valid_codes, len);
}

template <
    category cat,
    std::enable_if_t<cat == category::disconnect, bool> = true
>
std::pair<reason_code*, size_t> valid_codes() {
    static reason_code valid_codes[] = {
        normal_disconnection, unspecified_error,
        malformed_packet, protocol_error,
        implementation_specific_error, not_authorized,
        server_busy, server_shutting_down,
        keep_alive_timeout, session_taken_over,
        topic_filter_invalid, topic_name_invalid,
        receive_maximum_exceeded, topic_alias_invalid,
        packet_too_large, message_rate_too_high,
        quota_exceeded, administrative_action,
        payload_format_invalid, retain_not_supported,
        qos_not_supported, use_another_server,
        server_moved, shared_subscriptions_not_supported,
        connection_rate_exceeded, maximum_connect_time,
        subscription_ids_not_supported,
        wildcard_subscriptions_not_supported
    };
    static size_t len = sizeof(valid_codes) / sizeof(reason_code);
    return std::make_pair(valid_codes, len);
}


} // end namespace detail
} // end namespace reason_codes


template <reason_codes::category cat>
std::optional<reason_code> to_reason_code(uint8_t code) {
    auto [ptr, len] = reason_codes::detail::valid_codes<cat>();
    auto it = std::lower_bound(ptr, ptr + len, reason_code(code));

    if (it->value() == code)
        return *it;
    return std::nullopt;
}

} // end namespace boost::mqtt5

#endif // !BOOST_MQTT5_REASON_CODES_HPP
