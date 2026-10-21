//
// Copyright (c) 2023-2026 Ivica Siladic, Bruno Iljazovic, Korina Simicevic
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MQTT5_MQTT_CLIENT_HPP
#define BOOST_MQTT5_MQTT_CLIENT_HPP

#include <boost/mqtt5/error.hpp>
#include <boost/mqtt5/logger_traits.hpp>
#include <boost/mqtt5/types.hpp>

#include <boost/mqtt5/detail/log_invoke.hpp>
#include <boost/mqtt5/detail/rebind_executor.hpp>

#include <boost/mqtt5/impl/client_service.hpp>
#include <boost/mqtt5/impl/publish_send_op.hpp>
#include <boost/mqtt5/impl/re_auth_op.hpp>
#include <boost/mqtt5/impl/run_op.hpp>
#include <boost/mqtt5/impl/subscribe_op.hpp>
#include <boost/mqtt5/impl/unsubscribe_op.hpp>

#include <boost/asio/async_result.hpp>
#include <boost/system/error_code.hpp>

#include <memory>
#include <string>
#include <type_traits>
#include <variant> // std::monostate
#include <vector>

namespace boost::mqtt5 {

namespace asio = boost::asio;

/**
 * \brief \__MQTT\__ client used to connect and communicate with a Broker.
 *
 * \tparam \__StreamType\__ Type of the underlying transport protocol used to transfer
 * the stream of bytes between the Client and the Broker. The transport must be
 * ordered and lossless.
 * \tparam \__TlsContext\__ Type of the context object used in TLS/SSL connections.
 * \tparam \__LoggerType\__ Type of object used to log events within the Client.
 *
 * \par Thread safety
 * Distinct objects: safe. \n
 * Shared objects: unsafe. \n
 * This class is <b>not thread-safe</b>.
 * The application must also ensure that all asynchronous operations are performed within the same implicit or explicit strand.
 */
template <
    typename StreamType,
    typename TlsContext = std::monostate,
    typename LoggerType = noop_logger
>
class mqtt_client {
public:
    /// The executor type associated with the client.
    using executor_type = typename StreamType::executor_type;

    /// Rebinds the client type to another executor.
    template <typename Executor>
    struct rebind_executor {
        /// The client type when rebound to the specified executor.
        using other = mqtt_client<
            typename detail::rebind_executor<StreamType, Executor>::other,
            TlsContext,
            LoggerType
        >;
    };

private:
    using stream_type = StreamType;
    using tls_context_type = TlsContext;
    using logger_type = LoggerType;

    using client_service_type = detail::client_service<
        stream_type, tls_context_type, logger_type
    >;
    using impl_type = std::shared_ptr<client_service_type>;
    impl_type _impl;

public:
    /**
     * \brief Constructs a Client with given parameters.
     *
     * \param ex An executor that will be associated with the Client.
     * \param tls_context A context object used in TLS/SSL connection.
     * \param logger An object satisfying the \__LoggerType\__ concept used to log events within the Client.
     */
    explicit mqtt_client(
        const executor_type& ex,
        tls_context_type tls_context = {}, logger_type logger = {}
    ) :
        _impl(std::make_shared<client_service_type>(
            ex, std::move(tls_context), std::move(logger)
        ))
    {}

    /**
     * \brief Constructs a Client with given parameters.
     *
     * \tparam \__ExecutionContext\__ Type of a concrete execution context.
     * \param context Execution context whose executor will be associated with the Client.
     * \param tls_context A context object used in TLS/SSL connection.
     * \param logger An object satisfying the \__LoggerType\__ concept used to log events within the Client.
     *
     * \par Precondition
     * \code
     * std::is_convertible_v<ExecutionContext&, asio::execution_context&>
     * \endcode
     */
    template <
        typename ExecutionContext,
        std::enable_if_t<
            std::is_convertible_v<ExecutionContext&, asio::execution_context&>, 
            bool
        > = true
    >
    explicit mqtt_client(
        ExecutionContext& context,
        tls_context_type tls_context = {}, logger_type logger = {}
    ) :
        mqtt_client(
            context.get_executor(),
            std::move(tls_context), std::move(logger)
        )
    {}

    /**
     * \brief Move-construct an mqtt_client from another.
     */
    mqtt_client(mqtt_client&&) noexcept = default;

    /**
     * \brief Move assignment operator.
     *
     * \details Cancels this client first.
     */
    mqtt_client& operator=(mqtt_client&& other) noexcept {
        _impl->cancel();
        _impl = std::move(other._impl);
        return *this;
    }

    /**
     * \brief Destructor.
     *
     * \details Automatically calls \ref cancel.
     */
    ~mqtt_client() {
        if (_impl)
            _impl->cancel();
    }

    /**
     * \brief Get the executor associated with the object.
     */
    executor_type get_executor() const noexcept {
        return _impl->get_executor();
    }

    /**
     * \brief Get the context object used in TLS/SSL connection.
     *
     * \note This function may only be invoked
     * when the template parameter \__TlsContext\__ was configured
     * with non-default type during the creation of a \ref mqtt_client.
     *
     * \par Precondition
     * \code
     * !std::is_same_v<TlsContext, std::monostate>
     * \endcode
     */
    template <
        typename Ctx = TlsContext,
        std::enable_if_t<!std::is_same_v<Ctx, std::monostate>, bool> = true
    >
    decltype(auto) tls_context() {
        return _impl->tls_context();
    }

    /**
     * \brief Start the Client.
     *
     * \param token Completion token that will be used to produce a
     * completion handler. The handler will be invoked when the operation completes.
     *
     * \par Handler signature
     * The handler signature for this operation:
     *    \code
     *        void (__ERROR_CODE__)
     *    \endcode
     *
     * \par Completion condition
     * The asynchronous operation will complete with
     * `boost::asio::error::operation_aborted` when the client is cancelled by calling
     * \ref async_disconnect, \ref cancel, destruction or
     * if a non-recoverable error happens during a connection attempt (e.g. access denied).
     * The operation will complete immediately with \ref client::error::already_running
     * if the Client is already running.
     *
     *    \par Error codes
     *    The list of all possible error codes that this operation can finish with:\n
     *        - `boost::asio::error::operation_aborted`\n
     *        - \ref client::error::already_running\n
     *
     *    \par Per-Operation Cancellation
     *    This asynchronous operation supports cancellation for the following \__CANCELLATION_TYPE\__ values:\n
     *        - `cancellation_type::terminal` - invokes \ref cancel \n
     */
    template <
        typename CompletionToken =
            typename asio::default_completion_token<executor_type>::type
    >
    decltype(auto) async_run(CompletionToken&& token = {}) {
        using Signature = void (error_code);
        return asio::async_initiate<CompletionToken, Signature>(
            detail::initiate_async_run(_impl), token
        );
    }

    /**
     * \brief Cancel all asynchronous operations. This function has terminal effects.
     *
     * \details All outstanding operations will complete
     * with `boost::asio::error::operation_aborted`.
     *
     * \attention This function has terminal effects and will close the Client.
     * The Client cannot be used before calling \ref async_run again.
     */
    void cancel() {
        auto impl = _impl;
        _impl = impl->dup();
        impl->cancel();
    }

    /**
     * \brief Assign a \ref will Message.
     *
     * \details The \ref will Message that the Broker should publish
     * after the Network Connection is closed and it is not
     * closed normally.
     *
     * \attention This function takes action when the client is in a non-operational state,
     * meaning the \ref async_run function has not been invoked.
     * Furthermore, you can use this function after the \ref cancel function has been called,
     * before the \ref async_run function is invoked again.
     */
    mqtt_client& will(will will) {
        _impl->will(std::move(will));
        return *this;
    }

    /**
     * \brief Assign credentials that will be used to connect to a Broker.
     *
     * \details Credentials consist of a unique Client Identifier and, optionally,
     * a User Name and Password.
     *
     * \attention This function takes action when the client is in a non-operational state,
     * meaning the \ref async_run function has not been invoked.
     * Furthermore, you can use this function after the \ref cancel function has been called,
     * before the \ref async_run function is invoked again.
     */
    mqtt_client& credentials(
        std::string client_id,
        std::string username = "", std::string password = ""
    ) {
        _impl->credentials(
            std::move(client_id),
            std::move(username), std::move(password)
        );
        return *this;
    }

    /**
     * \brief Assign a list of Brokers that the Client will attempt to connect to.
     *
     * \details The Client will cycle through the list of hosts,
     * attempting to establish a connection with each
     * until it successfully establishes a connection.
     *
     * \param hosts List of Broker addresses and ports.
     * Address and ports are separated with a colon `:` while
     * pairs of addresses and ports are separated with a comma `,`.
     * \param default_port The default port to connect to in case the port is not
     * explicitly specified in the `hosts` list.
     *
     * \attention This function takes action when the client is in a non-operational state,
     * meaning the \ref async_run function has not been invoked.
     * Furthermore, you can use this function after the \ref cancel function has been called,
     * before the \ref async_run function is invoked again.
     *
     * \par Example
     * Some valid `hosts` string:
     *
     * \code
     *        std::string valid_hosts_1 = "broker1:1883, broker2, broker3:1883";
     *        std::string valid_hosts_2 = "broker1";
     * \endcode
     *
     */
    mqtt_client& brokers(std::string hosts, uint16_t default_port = 1883) {
        _impl->brokers(std::move(hosts), default_port);
        return *this;
    }

    /**
     * \brief Assign an authenticator that the Client will use for
     * \__ENHANCED_AUTH\__ on every connect to a Broker.
     * Re-authentication can be initiated by calling \ref re_authenticate.
     *
     * \param authenticator Object that will be stored (move-constructed or by reference)
     * and used for authentication. It needs to satisfy \__Authenticator\__ concept.
     *
     * \attention This function takes action when the client is in a non-operational state,
     * meaning the \ref async_run function has not been invoked.
     * Furthermore, you can use this function after the \ref cancel function has been called,
     * before the \ref async_run function is invoked again.
     *
     */
    template <typename Authenticator>
    mqtt_client& authenticator(Authenticator&& authenticator) {
        static_assert(
            detail::is_authenticator<Authenticator>,
            "The type does not satisfy the Authenticator concept"
        );
        _impl->authenticator(std::forward<Authenticator>(authenticator));
        return *this;
    }

    /**
     * \brief Assign the maximum time interval that is permitted to elapse between
     * two transmissions from the Client.
     *
     * \details A non-zero value initiates a process of sending a \__PINGREQ\__
     * packet every `seconds`. If this function is not invoked, the Client assumes
     * a \__KEEP_ALIVE\__ interval of 60 seconds.
     *
     * \param seconds Time interval in seconds.
     *
     * \note If the Server sends a \__SERVER_KEEP_ALIVE\__,
     * the Client will send a \__PINGREQ\__ packet every \__SERVER_KEEP_ALIVE\__ seconds.
     *
     * \attention This function takes action when the client is in a non-operational state,
     * meaning the \ref async_run function has not been invoked.
     * Furthermore, you can use this function after the \ref cancel function has been called,
     * before the \ref async_run function is invoked again.
     *
     */
    mqtt_client& keep_alive(uint16_t seconds) {
        _impl->keep_alive(seconds);
        return *this;
    }

    /**
     * \brief Assign \__CONNECT_PROPS\__ that will be sent in a \__CONNECT\__ packet.
     * \param props \__CONNECT_PROPS\__ sent in a \__CONNECT\__ packet.
     * \see See \__CONNECT_PROPS\__ for all eligible properties.
     */
    mqtt_client& connect_properties(connect_props props) {
        _impl->connect_properties(std::move(props));
        return *this;
    }

    /**
     * \brief Assign a property that will be sent in a \__CONNECT\__ packet.
     * \param prop The \__CONNECT_PROPS\__ property to set.
     * \param value Value that will be assigned to the property.
     *
     * \par Example
     * \code
     * client.connect_property(prop::session_expiry_interval, 40); // ok
     * client.connect_property(prop::reason_string, "reason"); // does not compile, not a CONNECT prop!
     * \endcode
     *
     * \see See \__CONNECT_PROPS\__ for all eligible properties.
     */
    template <prop::property_type p>
    mqtt_client& connect_property(
        std::integral_constant<prop::property_type, p> prop,
        prop::value_type_t<p> value
    ) {
        _impl->connect_property(prop, std::move(value));
        return *this;
    }

    /**
     * \brief Initiates \__RE_AUTHENTICATION\__
     * using the authenticator given in the \ref authenticator method.
     *
     * \note If \ref authenticator was not called, this method does nothing.
     */
    void re_authenticate() {
        detail::re_auth_op { _impl }.perform();
    }

    /**
     * \brief Retrieves the value of a specific property from the last \__CONNACK\__ packet received.
     *
     * \details The return type varies according to the property requested.
     * For all properties, the return type will be `std::optional` of their respective value type.
     * For `boost::mqtt5::prop::user_property`, the return type is
     * `std::vector<std::pair<std::string, std::string>>`.
     *
     * \param prop The \__CONNACK_PROPS\__ property value to retrieve.
     *
     * \par Example
     * \code
     *    std::optional<std::string> auth_method = client.connack_property(boost::mqtt5::prop::authentication_method); // ok
     *    std::optional<std::string> c_type = client.connack_property(boost::mqtt5::prop::content_type); // does not compile, not a CONNACK prop!
     * \endcode
     *
     * \see See \__CONNACK_PROPS\__ for all eligible properties.
     */
    template <prop::property_type p>
    const auto& connack_property(
        std::integral_constant<prop::property_type, p> prop
    ) const {
        return _impl->connack_property(prop);
    }

    /**
     * \brief Retrieves the \__CONNACK_PROPS\__ from the last \__CONNACK\__ packet received.
     *
     * \see See \__CONNACK_PROPS\__ for all eligible properties.
     */
    const connack_props& connack_properties() const {
        return _impl->connack_properties();
    }

    /**
     * \brief Send a \__PUBLISH\__ packet to Broker to transport an
     * Application Message.
     *
     * \tparam qos_type The \ref qos_e level of assurance for delivery.
     * \param topic Identification of the information channel to which
     * Payload data is published.
     * \param payload The Application Message that is being published.
     * \param retain The \ref retain_e flag.
     * \param props An instance of \__PUBLISH_PROPS\__. 
     * \param token Completion token that will be used to produce a
     * completion handler. The handler will be invoked when the operation completes.
     * On immediate completion, invocation of the handler will be performed in a manner
     * equivalent to using \__ASYNC_IMMEDIATE\__.
     *
     * \par Handler signature
     * The handler signature for this operation depends on the \ref qos_e specified:\n
     *
     *    `qos` == `qos_e::at_most_once`:
     *        \code
     *            void (
     *                __ERROR_CODE__    // Result of operation
     *            )
     *        \endcode
     *
     *    `qos` == `qos_e::at_least_once`:
     *        \code
     *            void (
     *                __ERROR_CODE__,    // Result of operation.
     *                __REASON_CODE__,   // Reason Code received from Broker.
     *                __PUBACK_PROPS__   // Properties received in the PUBACK packet.
     *            )
     *        \endcode
     *
     *    `qos` == `qos_e::exactly_once`:
     *        \code
     *            void (
     *                __ERROR_CODE__,    // Result of operation.
     *                __REASON_CODE__,   // Reason Code received from Broker.
     *                __PUBCOMP_PROPS__  // Properties received in the PUBCOMP packet.
     *            )
     *        \endcode
     *
     *    \par Completion condition
     *    Depending on the \ref qos_e specified, the asynchronous operation will complete
     *    when one of the following conditions is true:\n
     *        - If `qos` == `qos_e::at_most_once` and the Client
     *        has successfully written the packet to the transport. \n
     *        - If `qos` == `qos_e::at_least_once` and the packet has
     *        been sent and acknowledged through the reception of a \__PUBACK\__ packet.
     *        - If `qos` == `qos_e::exactly_once` and the packet has
     *        been sent and fully acknowledged through the reception of a \__PUBCOMP\__ packet.
     *        - An error occurred. This is indicated by an associated \__ERROR_CODE\__ in the handler.\n
     *
     *    \par Error codes
     *    The list of all possible error codes that this operation can finish with:\n
     *        - `boost::system::errc::errc_t::success` \n
     *        - `boost::asio::error::operation_aborted` \n
     *        - `boost::asio::error::no_recovery` \n
     *        - \ref boost::mqtt5::client::error::malformed_packet
     *        - \ref boost::mqtt5::client::error::packet_too_large
     *        - \ref boost::mqtt5::client::error::pid_overrun
     *        - \ref boost::mqtt5::client::error::qos_not_supported
     *        - \ref boost::mqtt5::client::error::retain_not_available
     *        - \ref boost::mqtt5::client::error::topic_alias_maximum_reached
     *        - \ref boost::mqtt5::client::error::invalid_topic
     *
     * Refer to the section on \__ERROR_HANDLING\__ to find the underlying causes for each error code.
     * 
     *    \par Per-Operation Cancellation
     *    This asynchronous operation supports cancellation for the following \__CANCELLATION_TYPE\__ values:\n
     *        - `cancellation_type::terminal` - invokes \ref cancel \n
     *        - `cancellation_type::partial` & `cancellation_type::total` - prevents potential resending of the \__PUBLISH\__ packet \n
     *
     */
    template <qos_e qos_type, 
        typename CompletionToken =
            typename asio::default_completion_token<executor_type>::type
    >
    decltype(auto) async_publish(
        std::string topic, std::string payload,
        retain_e retain, const publish_props& props,
        CompletionToken&& token = {}
    ) {
        using Signature = detail::on_publish_signature<qos_type>;
        return asio::async_initiate<CompletionToken, Signature>(
            detail::initiate_async_publish<client_service_type, qos_type>(_impl),
            token,
            std::move(topic), std::move(payload), retain, props
        );
    }

    /**
     * \brief Send a \__SUBSCRIBE\__ packet to Broker to create a Subscription
     * to one or more Topics of interest.
     *
     * \details After the Subscription has been established, the Broker will send
     * PUBLISH packets to the Client to forward Application Messages that were published
     * to Topics that the Client subscribed to. The Application Messages can be received
     * with \ref mqtt_client::async_receive function.
     *
     * \param topics A list of \ref subscribe_topic of interest.
     * \param props An instance of \__SUBSCRIBE_PROPS\__.
     * \param token Completion token that will be used to produce a
     * completion handler. The handler will be invoked when the operation completes.
     * On immediate completion, invocation of the handler will be performed in a manner
     * equivalent to using \__ASYNC_IMMEDIATE\__.
     *
     * \par Handler signature
     * The handler signature for this operation:
     *    \code
     *        void (
     *            __ERROR_CODE__,    // Result of operation.
     *            std::vector<__REASON_CODE__>,  // Vector of Reason Codes indicating
     *                                           // the Subscription result for each Topic
     *                                           // in the SUBSCRIBE packet.
     *            __SUBACK_PROPS__,  // Properties received in the SUBACK packet.
     *        )
     *    \endcode
     *
     *    \par Completion condition
     *    The asynchronous operation will complete when one of the following conditions is true:\n
     *        - The Client has successfully sent a \__SUBSCRIBE\__ packet
     *        and has received a \__SUBACK\__ response from the Broker.\n
     *        - An error occurred. This is indicated by an associated \__ERROR_CODE\__ in the handler.\n
     *
     *    \par Error codes
     *    The list of all possible error codes that this operation can finish with:\n
     *        - `boost::system::errc::errc_t::success` \n
     *        - `boost::asio::error::no_recovery` \n
     *        - `boost::asio::error::operation_aborted` \n
     *        - \ref boost::mqtt5::client::error::malformed_packet
     *        - \ref boost::mqtt5::client::error::packet_too_large
     *        - \ref boost::mqtt5::client::error::pid_overrun
     *        - \ref boost::mqtt5::client::error::invalid_topic
     *        - \ref boost::mqtt5::client::error::wildcard_subscription_not_available
     *        - \ref boost::mqtt5::client::error::subscription_identifier_not_available
     *        - \ref boost::mqtt5::client::error::shared_subscription_not_available
     *
     * Refer to the section on \__ERROR_HANDLING\__ to find the underlying causes for each error code.
     * 
     *    \par Per-Operation Cancellation
     *    This asynchronous operation supports cancellation for the following \__CANCELLATION_TYPE\__ values:\n
     *        - `cancellation_type::terminal` - invokes \ref mqtt_client::cancel \n
     *        - `cancellation_type::partial` & `cancellation_type::total` - prevents potential resending of the \__SUBSCRIBE\__ packet \n
     *
     */
    template <
        typename CompletionToken =
            typename asio::default_completion_token<executor_type>::type
    >
    decltype(auto) async_subscribe(
        const std::vector<subscribe_topic>& topics,
        const subscribe_props& props,
        CompletionToken&& token = {}
    ) {
        using Signature = void (
            error_code, std::vector<reason_code>, suback_props
        );
        return asio::async_initiate<CompletionToken, Signature>(
            detail::initiate_async_subscribe(_impl), token,
            topics, props
        );
    }

    /**
     * \brief Send a \__SUBSCRIBE\__ packet to Broker to create a Subscription
     * to one Topic of interest.
     *
     * \details After the Subscription has been established, the Broker will send
     * \__PUBLISH\__ packets to the Client to forward Application Messages that were published
     * to Topics that the Client subscribed to. The Application Messages can be received
     * with \ref mqtt_client::async_receive function.
     *
     * \param topic A \ref subscribe_topic of interest.
     * \param props An instance of \__SUBSCRIBE_PROPS\__.
     * \param token Completion token that will be used to produce a
     * completion handler. The handler will be invoked when the operation completes.
     * On immediate completion, invocation of the handler will be performed in a manner
     * equivalent to using \__ASYNC_IMMEDIATE\__.
     *
     * \par Handler signature
     * The handler signature for this operation:
     *    \code
     *        void (
     *            __ERROR_CODE__,    // Result of operation.
     *            std::vector<__REASON_CODE__>,  // Vector of Reason Codes containing the
     *                                           // single Subscription result for the Topic
     *                                           // in the SUBSCRIBE packet.
     *            __SUBACK_PROPS__,  // Properties received in the SUBACK packet.
     *        )
     *    \endcode
     *
     *    \par Completion condition
     *    The asynchronous operation will complete when one of the following conditions is true:\n
     *        - The Client has successfully sent a \__SUBSCRIBE\__ packet
     *        and has received a \__SUBACK\__ response from the Broker.\n
     *        - An error occurred. This is indicated by an associated \__ERROR_CODE\__ in the handler.\n
     *
     *    \par Error codes
     *    The list of all possible error codes that this operation can finish with:\n
     *        - `boost::system::errc::errc_t::success` \n
     *        - `boost::asio::error::no_recovery` \n
     *        - `boost::asio::error::operation_aborted` \n
     *        - \ref boost::mqtt5::client::error::malformed_packet
     *        - \ref boost::mqtt5::client::error::packet_too_large
     *        - \ref boost::mqtt5::client::error::pid_overrun
     *        - \ref boost::mqtt5::client::error::invalid_topic
     *        - \ref boost::mqtt5::client::error::wildcard_subscription_not_available
     *        - \ref boost::mqtt5::client::error::subscription_identifier_not_available
     *        - \ref boost::mqtt5::client::error::shared_subscription_not_available
     *
     * Refer to the section on \__ERROR_HANDLING\__ to find the underlying causes for each error code.
     * 
     *    \par Per-Operation Cancellation
     *    This asynchronous operation supports cancellation for the following \__CANCELLATION_TYPE\__ values:\n
     *        - `cancellation_type::terminal` - invokes \ref mqtt_client::cancel \n
     *        - `cancellation_type::partial` & `cancellation_type::total` - prevents potential resending of the \__SUBSCRIBE\__ packet \n
     *
     */
    template <
        typename CompletionToken =
            typename asio::default_completion_token<executor_type>::type
    >
    decltype(auto) async_subscribe(
        const subscribe_topic& topic, const subscribe_props& props,
        CompletionToken&& token = {}
    ) {
        return async_subscribe(
            std::vector<subscribe_topic> { topic }, props,
            std::forward<CompletionToken>(token)
        );
    }


    /**
     * \brief Send an \__UNSUBSCRIBE\__ packet to Broker to unsubscribe from one
     * or more Topics.
     *
     * \note The Client may still receive residual Application Messages
     * through the \ref mqtt_client::async_receive function
     * from Topics the Client just unsubscribed to.
     *
     * \param topics List of Topics to unsubscribe from.
     * \param props An instance of \__UNSUBSCRIBE_PROPS\__.
     * \param token Completion token that will be used to produce a
     * completion handler. The handler will be invoked when the operation completes.
     * On immediate completion, invocation of the handler will be performed in a manner
     * equivalent to using \__ASYNC_IMMEDIATE\__.
     *
     * \par Handler signature
     * The handler signature for this operation:
     *    \code
     *        void (
     *            __ERROR_CODE__, // Result of operation.
     *            std::vector<__REASON_CODE__>,  // Vector of Reason Codes indicating
     *                                           // the result of unsubscribe operation
     *                                           // for each Topic in the UNSUBSCRIBE packet.
     *            __UNSUBACK_PROPS__, // Properties received in the UNSUBACK packet.
     *        )
     *    \endcode
     *
     *    \par Completion condition
     *    The asynchronous operation will complete when one of the following conditions is true:\n
     *        - The Client has successfully sent an \__UNSUBSCRIBE\__ packet
     *        and has received an \__UNSUBACK\__ response from the Broker.\n
     *        - An error occurred. This is indicated by an associated \__ERROR_CODE\__ in the handler.\n
     *
     *    \par Error codes
     *    The list of all possible error codes that this operation can finish with:\n
     *        - `boost::system::errc::errc_t::success` \n
     *        - `boost::asio::error::no_recovery` \n
     *        - `boost::asio::error::operation_aborted` \n
     *        - \ref boost::mqtt5::client::error::malformed_packet
     *        - \ref boost::mqtt5::client::error::packet_too_large
     *        - \ref boost::mqtt5::client::error::pid_overrun
     *        - \ref boost::mqtt5::client::error::invalid_topic
     *
     * Refer to the section on \__ERROR_HANDLING\__ to find the underlying causes for each error code.
     * 
     *    \par Per-Operation Cancellation
     *    This asynchronous operation supports cancellation for the following \__CANCELLATION_TYPE\__ values:\n
     *        - `cancellation_type::terminal` - invokes \ref mqtt_client::cancel \n
     *        - `cancellation_type::partial` & `cancellation_type::total` - prevents potential resending of the \__UNSUBSCRIBE\__ packet \n
     *
     */
    template <
        typename CompletionToken =
            typename asio::default_completion_token<executor_type>::type
    >
    decltype(auto) async_unsubscribe(
        const std::vector<std::string>& topics,    const unsubscribe_props& props,
        CompletionToken&& token = {}
    ) {
        using Signature = void (
            error_code, std::vector<reason_code>, unsuback_props
        );
        return asio::async_initiate<CompletionToken, Signature>(
            detail::initiate_async_unsubscribe(_impl), token,
            topics, props
        );
    }

    /**
     * \brief Send an \__UNSUBSCRIBE\__ packet to Broker to unsubscribe
     * from one Topic.
     *
     * \note The Client may still receive residual Application Messages
     * through the \ref mqtt_client::async_receive function
     * from Topics the Client just unsubscribed to.
     *
     * \param topic Topic to unsubscribe from.
     * \param props An instance of \__UNSUBSCRIBE_PROPS\__.
     * \param token Completion token that will be used to produce a
     * completion handler. The handler will be invoked when the operation completes.
     * On immediate completion, invocation of the handler will be performed in a manner
     * equivalent to using \__ASYNC_IMMEDIATE\__.
     *
     * \par Handler signature
     * The handler signature for this operation:
     *    \code
     *        void (
     *            __ERROR_CODE__, // Result of operation.
     *            std::vector<__REASON_CODE__>,  // Vector of Reason Codes containing
     *                                           // the result of unsubscribe operation
     *                                           // for the Topic in the UNSUBSCRIBE packet.
     *            __UNSUBACK_PROPS__, // Properties received in the UNSUBACK packet.
     *        )
     *    \endcode
     *
     *    \par Completion condition
     *    The asynchronous operation will complete when one of the following conditions is true:\n
     *        - The Client has successfully sent an \__UNSUBSCRIBE\__ packet
     *        and has received an \__UNSUBACK\__ response from the Broker.\n
     *        - An error occurred. This is indicated by an associated \__ERROR_CODE\__ in the handler.\n
     *
     *    \par Error codes
     *    The list of all possible error codes that this operation can finish with:\n
     *        - `boost::system::errc::errc_t::success` \n
     *        - `boost::asio::error::no_recovery` \n
     *        - `boost::asio::error::operation_aborted` \n
     *        - \ref boost::mqtt5::client::error::malformed_packet
     *        - \ref boost::mqtt5::client::error::packet_too_large
     *        - \ref boost::mqtt5::client::error::pid_overrun
     *        - \ref boost::mqtt5::client::error::invalid_topic
     *
     * Refer to the section on \__ERROR_HANDLING\__ to find the underlying causes for each error code.
     * 
     *    \par Per-Operation Cancellation
     *    This asynchronous operation supports cancellation for the following \__CANCELLATION_TYPE\__ values:\n
     *        - `cancellation_type::terminal` - invokes \ref mqtt_client::cancel \n
     *        - `cancellation_type::partial` & `cancellation_type::total` - prevents potential resending of the \__UNSUBSCRIBE\__ packet \n
     *
     */
    template <
        typename CompletionToken =
            typename asio::default_completion_token<executor_type>::type
    >
    decltype(auto) async_unsubscribe(
        const std::string& topic, const unsubscribe_props& props,
        CompletionToken&& token = {}
    ) {
        return async_unsubscribe(
            std::vector<std::string> { topic }, props,
            std::forward<CompletionToken>(token)
        );
    }


    /**
     * \brief Asynchronously receive an Application Message.
     *
     * \details The Client will receive and complete deliveries for all the
     * \__PUBLISH\__ packets received from the Broker throughout its lifetime.
     * The Client will store them internally in the order they were delivered.
     * Calling this function will attempt to receive an Application Message
     * from internal storage.
     *
     * \note It is only recommended to call this function if you have established
     * a successful Subscription to a Topic using the \ref async_subscribe function.
     *
     * \param token Completion token that will be used to produce a
     * completion handler. The handler will be invoked when the operation completes.
     * On immediate completion, invocation of the handler will be performed in a manner
     * equivalent to using \__POST\__.
     *
     * \par Handler signature
     * The handler signature for this operation:
     *    \code
     *        void (
     *            __ERROR_CODE__, // Result of operation.
     *            std::string,    // Topic, the origin of the Application Message.
     *            std::string,    // Payload, the content of the Application Message.
     *            __PUBLISH_PROPS__, // Properties received in the PUBLISH packet.
     *        )
     *    \endcode
     *
     * \par Completion condition
     *    The asynchronous operation will complete when one of the following conditions is true:\n
     *        - The Client has a pending Application Message in its internal storage
     *        ready to be received.
     *        - An error occurred. This is indicated by an associated \__ERROR_CODE\__ in the handler.\n
     *
     *    \par Error codes
     *    The list of all possible error codes that this operation can finish with:\n
     *        - `boost::system::errc::errc_t::success`\n
     *        - `boost::asio::error::operation_aborted`\n
     *        - \ref boost::mqtt5::client::error::session_expired
     *
     * Refer to the section on \__ERROR_HANDLING\__ to find the underlying causes for each error code.
     * 
     *    \par Per-Operation Cancellation
     *    This asynchronous operation supports cancellation for the following \__CANCELLATION_TYPE\__ values:\n
     *        - `cancellation_type::terminal` \n
     *        - `cancellation_type::partial` \n
     *        - `cancellation_type::total` \n
     */
    template <
        typename CompletionToken =
            typename asio::default_completion_token<executor_type>::type
    >
    decltype(auto) async_receive(CompletionToken&& token = {}) {
        return _impl->async_channel_receive(std::forward<CompletionToken>(token));
    }

    /**
     * \brief Disconnect the Client by sending a \__DISCONNECT\__ packet
     * with a specified Reason Code. This function has terminal effects.
     *
     * \details The Client will attempt to send a \__DISCONNECT\__ packet to the Broker
     * with a Reason Code describing the reason for disconnection.
     * If the \__DISCONNECT\__ packet is successfully transmitted,
     * or if `5 seconds` elapsed without a successful send, the Client will terminate the connection.
     *
     * \attention This function has terminal effects and will close the Client.
     * See \ref mqtt_client::cancel.
     *
     * \param reason_code Reason Code to notify
     * the Broker of the reason for the disconnection.
     * \param props An instance of \__DISCONNECT_PROPS\__.
     * \param token Completion token that will be used to produce a
     * completion handler. The handler will be invoked when the operation completes.
     *
     * \par Handler signature
     * The handler signature for this operation:
     *    \code
     *        void (
     *            __ERROR_CODE__ // Result of operation.
     *        )
     *    \endcode
     *
     *    \par Completion condition
     *    The asynchronous operation will complete when one of the following conditions is true:\n
     *        - The Client has sent a \__DISCONNECT\__ packet.\n
     *        - 5 seconds have elapsed without a successful send.\n
     *        - An error occurred. This is indicated by an associated \__ERROR_CODE\__ in the handler.\n
     *
     *    \par Error codes
     *    The list of all possible error codes that this operation can finish with:\n
     *        - `boost::system::errc::errc_t::success`\n
     *        - `boost::asio::error::operation_aborted`[footnote
                This error code can appear if the Client fails to send the \__DISCONNECT\__ packet to the Server.
                Regardless, the connection to the Server is terminated, and the Client is cancelled.
            ]\n
     *        - \ref boost::mqtt5::client::error::malformed_packet
     *
     * Refer to the section on \__ERROR_HANDLING\__ to find the underlying causes for each error code.
     * 
     *    \par Per-Operation Cancellation
     *    This asynchronous operation supports cancellation for the following \__CANCELLATION_TYPE\__ values:\n
     *        - `cancellation_type::terminal` - invokes \ref mqtt_client::cancel \n
     * 
     */
    template <
        typename CompletionToken =
            typename asio::default_completion_token<executor_type>::type
    >
    decltype(auto) async_disconnect(
        disconnect_rc_e reason_code, const disconnect_props& props,
        CompletionToken&& token = {}
    ) {
        auto impl = _impl;
        _impl = impl->dup();
        return detail::async_terminal_disconnect(
            detail::disconnect_rc_e(static_cast<uint8_t>(reason_code)),
            props, impl, std::forward<CompletionToken>(token)
        );
    }

    /**
     * \brief Disconnect the Client by sending a \__DISCONNECT\__ packet
     * with a Reason Code of reason_codes.normal_disconnection.
     * This function has terminal effects.
     * 
     * \details The Client will attempt to send a \__DISCONNECT\__ packet to the Broker
     * with a Reason Code describing the reason for disconnection.
     * If the \__DISCONNECT\__ packet is successfully transmitted,
     * or if `5 seconds` elapsed without a successful send, the Client will terminate the connection.
     *
     * \attention This function has terminal effects and will close the Client.
     * See \ref mqtt_client::cancel.
     *
     * \param token Completion token that will be used to produce a
     * completion handler. The handler will be invoked when the operation completes.
     *
     * \par Handler signature
     * The handler signature for this operation:
     *    \code
     *        void (
     *            __ERROR_CODE__ // Result of operation.
     *        )
     *    \endcode
     *
     *    \par Completion condition
     *    The asynchronous operation will complete when one of the following conditions is true:\n
     *        - The Client has attempted to send a \__DISCONNECT\__ packet, regardless of whether
     *        the sending was successful or not.\n
     *        - An error occurred. This is indicated by an associated \__ERROR_CODE\__ in the handler.\n
     *
     *    \par Error codes
     *    The list of all possible error codes that this operation can finish with:\n
     *        - `boost::system::errc::errc_t::success`\n
     *        - `boost::asio::error::operation_aborted`[footnote
                This error code can appear if the Client fails to send the \__DISCONNECT\__ packet to the Server.
                Regardless, the connection to the Server is terminated, and the Client is cancelled.
            ]\n
     *        - \ref boost::mqtt5::client::error::malformed_packet
     *
     * Refer to the section on \__ERROR_HANDLING\__ to find the underlying causes for each error code.
     *
     *    \par Per-Operation Cancellation
     *    This asynchronous operation supports cancellation for the following \__CANCELLATION_TYPE\__ values:\n
     *        - `cancellation_type::terminal` - invokes \ref mqtt_client::cancel \n
     */
    template <
        typename CompletionToken =
            typename asio::default_completion_token<executor_type>::type
    >
    decltype(auto) async_disconnect(CompletionToken&& token = {}) {
        return async_disconnect(
            disconnect_rc_e::normal_disconnection,
            disconnect_props {}, std::forward<CompletionToken>(token)
        );
    }

};

} // end namespace boost::mqtt5

#endif // !BOOST_MQTT5_MQTT_CLIENT_HPP
