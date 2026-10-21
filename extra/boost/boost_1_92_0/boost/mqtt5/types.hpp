//
// Copyright (c) 2023-2026 Ivica Siladic, Bruno Iljazovic, Korina Simicevic
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MQTT5_TYPES_HPP
#define BOOST_MQTT5_TYPES_HPP

#include <boost/mqtt5/property_types.hpp>

#include <boost/system/error_code.hpp>

#include <cstdint>
#include <string>

namespace boost::mqtt5 {

/// An alias for `boost::system::error_code`.
using error_code = boost::system::error_code;

/**
 * \brief A data structure used to store information related to an authority
 * such as the hostname, port, and path.
 */
struct authority_path {
    /** \brief The hostname of the authority as a domain name or an IP address. */
    std::string host;

    /** \brief The port number used for communication. */
    std::string port;

    /** \brief Specifies the endpoint path relevant to WebSocket connections. */
    std::string path;
};

/**
 * \brief Represents the Quality of Service (\__QOS__\)
 * property of the \__PUBLISH\__ packets.
 *
 * \details Determines how the \__PUBLISH\__ packets are delivered
 * from the sender to the receiver.
 */
enum class qos_e : std::uint8_t {
    /** \brief The message arrives at the receiver either once or not at all. */
    at_most_once = 0b00,

    /** \brief Ensures the message arrives at the receiver at least once. */
    at_least_once = 0b01,

    /** \brief All messages arrive at the receiver exactly once without
     loss or duplication of the messages. */
    exactly_once = 0b10
};

/**
 * \brief Represents the \__RETAIN\__ flag in the \__PUBLISH\__ packets.
 *
 * \details This flag informs the Server about whether or not it should
 * store the current message.
 */
enum class retain_e : std::uint8_t {
    /** \brief The Server will replace any existing retained message for this Topic
     with this message. */
    yes = 0b1,

    /** \brief The Server will not store this message and will not remove or replace
     any existing retained message. */
    no = 0b0
};

enum class dup_e : std::uint8_t {
    yes = 0b1, no = 0b0
};

/**
 * \brief Represents the stage of \__ENHANCED_AUTH\__ process.
 */
enum class auth_step_e {
    /** \brief The Client needs to send initial authentication data. */
    client_initial,

    /** \brief Server responded with \ref reason_codes::continue_authentication and possibly
     * authentication data, the Client needs to send further authentication data.
     */
    server_challenge,

    /** \brief Server responded with \ref reason_codes::success and final
     * authentication data, which the Client validates.
     */
    server_final
};

/**
 * \brief Representation of the No Local Subscribe Option.
 *
 * \details A Subscribe Option indicating whether or not Application Messages
 * will be forwarded to a connection with a ClientID equal to the ClientID of the
 * publishing connection.
 */
enum class no_local_e : std::uint8_t {
    /** \brief Application Messages can be forwarded to a connection with equal ClientID. */
    no = 0b0,

    /** \brief  Application Messages MUST NOT be forwarded to a connection with equal ClientID. */
    yes = 0b1
};

/**
 * \brief Representation of the Retain As Published Subscribe Option.
 *
 * \details A Subscribe Option indicating whether or not Application Messages forwarded
 * using this Subscription keep the \__RETAIN\__ flag they were published with.
 */
enum class retain_as_published_e : std::uint8_t {
    /** \brief Application Messages have the \__RETAIN\__ flag set to 0. */
    dont = 0b0,

    /** \brief Application Messages keep the \__RETAIN\__ flag they were published with. */
    retain = 0b1
};

/**
 * \brief Representation of the Retain Handling Subscribe Option.
 *
 * \details A Subscribe Option specifying whether retained messages are sent
 * when the Subscription is established.
 */
enum class retain_handling_e : std::uint8_t {
    /** \brief Send retained messages at the time of subscribe. */
    send = 0b00,

    /** \brief Send retained message only if the Subscription does not currently exist. */
    new_subscription_only = 0b01,

    /** \brief Do not send retained messages at the time of subscribe. */
    not_send = 0b10
};

/**
 * \brief Represents the \__SUBSCRIBE_OPTIONS\__ associated with each Subscription.
 */
struct subscribe_options {
    /** \brief Maximum \__QOS\__ level at which the Server can send Application Messages to the Client. */
    qos_e max_qos = qos_e::exactly_once;

    /** \brief Option determining if Application Messages will be
    forwarded to a connection with an equal ClientID. */
    no_local_e no_local = no_local_e::yes;

    /** \brief Option determining if Application Message will keep their \__RETAIN\__ flag. */
    retain_as_published_e retain_as_published = retain_as_published_e::retain;

    /** \brief Option determining if retained messages are sent
    when the Subscription is established. */
    retain_handling_e retain_handling = retain_handling_e::new_subscription_only;
};

/**
 * \brief A representation of a Topic Subscription consisting of a Topic Filter and
 * Subscribe Options.
 */
struct subscribe_topic {
    /** \brief An UTF-8 Encoded String indicating the Topics to which the Client wants to subscribe. */
    std::string topic_filter;

    /** \brief The \ref subscribe_options associated with the Subscription. */
    subscribe_options sub_opts;
};

/// \cond internal

class connect_props : public prop::properties<
    prop::session_expiry_interval_t,
    prop::receive_maximum_t,
    prop::maximum_packet_size_t,
    prop::topic_alias_maximum_t,
    prop::request_response_information_t,
    prop::request_problem_information_t,
    prop::user_property_t,
    prop::authentication_method_t,
    prop::authentication_data_t
> {};

class connack_props : public prop::properties<
    prop::session_expiry_interval_t,
    prop::receive_maximum_t,
    prop::maximum_qos_t,
    prop::retain_available_t,
    prop::maximum_packet_size_t,
    prop::assigned_client_identifier_t,
    prop::topic_alias_maximum_t,
    prop::reason_string_t,
    prop::user_property_t,
    prop::wildcard_subscription_available_t,
    prop::subscription_identifier_available_t,
    prop::shared_subscription_available_t,
    prop::server_keep_alive_t,
    prop::response_information_t,
    prop::server_reference_t,
    prop::authentication_method_t,
    prop::authentication_data_t
> {};

class publish_props : public prop::properties<
    prop::payload_format_indicator_t,
    prop::message_expiry_interval_t,
    prop::content_type_t,
    prop::response_topic_t,
    prop::correlation_data_t,
    prop::subscription_identifier_t,
    prop::topic_alias_t,
    prop::user_property_t
> {};

// puback, pubcomp
class puback_props : public prop::properties<
    prop::reason_string_t,
    prop::user_property_t
> {};

class pubcomp_props : public prop::properties<
    prop::reason_string_t,
    prop::user_property_t
> {};

class pubrec_props : public prop::properties<
    prop::reason_string_t,
    prop::user_property_t
> {};

class pubrel_props : public prop::properties<
    prop::reason_string_t,
    prop::user_property_t
> {};


class subscribe_props : public prop::properties<
    prop::subscription_identifier_t,
    prop::user_property_t
> {};

class suback_props : public prop::properties<
    prop::reason_string_t,
    prop::user_property_t
> {};

class unsubscribe_props : public prop::properties<
    prop::user_property_t
> {};

class unsuback_props : public prop::properties<
    prop::reason_string_t,
    prop::user_property_t
> {};

class disconnect_props : public prop::properties<
    prop::session_expiry_interval_t,
    prop::reason_string_t,
    prop::user_property_t,
    prop::server_reference_t
> {};

class auth_props : public prop::properties<
    prop::authentication_method_t,
    prop::authentication_data_t,
    prop::reason_string_t,
    prop::user_property_t
> {};


class will_props : public prop::properties<
    prop::will_delay_interval_t,
    prop::payload_format_indicator_t,
    prop::message_expiry_interval_t,
    prop::content_type_t,
    prop::response_topic_t,
    prop::correlation_data_t,
    prop::user_property_t
>{};

/// \endcond

/**
 * \brief Represents the Will Message.
 *
 * \details A Will Message is an Application Message that
 * the Broker should publish after the Network Connection is closed
 * in cases where the Network Connection is not closed normally.
 */
class will : public will_props {
    std::string _topic;
    std::string _message;
    qos_e _qos; retain_e _retain;

public:
    /**
     * \brief Constructs an empty Will Message.
     *
     * \attention Do not use!
     * An empty Will Message results in an empty Topic Name which is not valid.
     * Internal uses only.
     */
    will() = default;

    /**
     * \brief Construct a Will Message.
     *
     * \param topic Topic, identification of the information channel to which
     * the Will Message will be published.
     * \param message The message that will be published.
     * \param qos The \ref qos_e level used when publishing the Will Message.
     * \param retain The \ref retain_e flag specifying if the Will Message
     * is to be retained when it is published.
     */
    will(
        std::string topic, std::string message,
        qos_e qos = qos_e::at_most_once, retain_e retain = retain_e::no
    ) :
        _topic(std::move(topic)), _message(std::move(message)),
        _qos(qos), _retain(retain)
    {}

    /**
     * \brief Construct a Will Message.
     *
     * \param topic Topic name, identification of the information channel to which
     * the Will Message will be published.
     * \param message The message that will be published.
     * \param qos The \ref qos_e level used when publishing the Will Message.
     * \param retain The \ref retain_e flag specifying if the Will Message
     * is to be retained when it is published.
     * \param props Will properties.
     */
    will(
        std::string topic, std::string message,
        qos_e qos, retain_e retain, will_props props
    ) :
        will_props(std::move(props)),
        _topic(std::move(topic)), _message(std::move(message)),
        _qos(qos), _retain(retain)
    {}

    /// Get the Topic Name.
    std::string_view topic() const {
        return _topic;
    }

    /// Get the Application Message.
    std::string_view message() const {
        return _message;
    }

    /// Get the \ref qos_e.
    constexpr qos_e qos() const {
        return _qos;
    }

    /// Get the \ref retain_e.
    constexpr retain_e retain() const {
        return _retain;
    }
};


} // end namespace boost::mqtt5

#endif // !BOOST_MQTT5_TYPES_HPP
