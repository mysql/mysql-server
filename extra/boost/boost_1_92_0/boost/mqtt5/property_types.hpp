//
// Copyright (c) 2023-2026 Ivica Siladic, Bruno Iljazovic, Korina Simicevic
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MQTT5_PROPERTY_TYPES_HPP
#define BOOST_MQTT5_PROPERTY_TYPES_HPP

#include <boost/container/small_vector.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <vector>

namespace boost::mqtt5::prop {

/// \cond internal
enum property_type : uint8_t {
    payload_format_indicator_t = 0x01,
    message_expiry_interval_t = 0x02,
    content_type_t = 0x03,
    response_topic_t = 0x08,
    correlation_data_t = 0x09,
    subscription_identifier_t = 0x0b,
    session_expiry_interval_t = 0x11,
    assigned_client_identifier_t = 0x12,
    server_keep_alive_t = 0x13,
    authentication_method_t = 0x15,
    authentication_data_t = 0x16,
    request_problem_information_t = 0x17,
    will_delay_interval_t = 0x18,
    request_response_information_t = 0x19,
    response_information_t = 0x1a,
    server_reference_t = 0x1c,
    reason_string_t = 0x1f,
    receive_maximum_t = 0x21,
    topic_alias_maximum_t = 0x22,
    topic_alias_t = 0x23,
    maximum_qos_t = 0x24,
    retain_available_t = 0x25,
    user_property_t = 0x26,
    maximum_packet_size_t = 0x27,
    wildcard_subscription_available_t = 0x28,
    subscription_identifier_available_t = 0x29,
    shared_subscription_available_t = 0x2a
};
/// \endcond

/**
 * \brief A class holding any number of Subscription Identifiers.
 * 
 * \details Subscription Identifier is an integer that can be set in \__SUBSCRIBE_PROPS\__ when subscribing to a Topic.
 *    Broker will store the mapping between that Subscription and the Subscription Identifier.
 *    When an incoming \__PUBLISH\__ message matches one or more Subscriptions, respective Subscription
 *    Identifiers will be forwarded in the \__PUBLISH_PROPS\__ of the received message.
 */
class alignas(8) subscription_identifiers :
    public boost::container::small_vector<int32_t, 1>
{
    using base_type = boost::container::small_vector<int32_t, 1>;

public:
    using base_type::base_type;

    /// Constructs Subscription Identifiers with given parameters.
    subscription_identifiers(int32_t val) : base_type { val } {}

    /// \{ Checks whether there are any Subscription Identifiers.
    bool has_value() const noexcept {
        return !empty();
    }

    explicit operator bool() const noexcept {
        return !empty();
    }
    /// \}

    /// \{ Accesses the (first) Subscription Identifier.
    int32_t& operator*() noexcept {
        return front();
    }

    int32_t operator*() const noexcept {
        return front();
    }
    /// \}

    /// Sets or replaces the (first) Subscription Identifier.
    void emplace(int32_t val = 0) {
        *this = val;
    }

    /// Returns the first Subscription Identifier.
    int32_t value() const {
        return front();
    }

    /// Returns the first Subscription Identifier if it exists, otherwise returns \p default_val
    int32_t value_or(int32_t default_val) const noexcept {
        return empty() ? default_val : front();
    }

    /// Clears the Subscription Identifiers.
    void reset() noexcept {
        clear();
    }
};

template <property_type p>
struct property_traits;

using user_property_value_t = std::vector<std::pair<std::string, std::string>>;

#define DEF_PROPERTY_TRAIT(Pname, Ptype) \
template <> \
struct property_traits<Pname##_t> { \
    static constexpr std::string_view name = #Pname; \
    using type = Ptype; \
}; \
constexpr std::integral_constant<property_type, Pname##_t> Pname {};

DEF_PROPERTY_TRAIT(payload_format_indicator, std::optional<uint8_t>);
DEF_PROPERTY_TRAIT(message_expiry_interval, std::optional<uint32_t>);
DEF_PROPERTY_TRAIT(content_type, std::optional<std::string>);
DEF_PROPERTY_TRAIT(response_topic, std::optional<std::string>);
DEF_PROPERTY_TRAIT(correlation_data, std::optional<std::string>);
DEF_PROPERTY_TRAIT(subscription_identifier, subscription_identifiers);
DEF_PROPERTY_TRAIT(session_expiry_interval, std::optional<uint32_t>);
DEF_PROPERTY_TRAIT(assigned_client_identifier, std::optional<std::string>);
DEF_PROPERTY_TRAIT(server_keep_alive, std::optional<uint16_t>);
DEF_PROPERTY_TRAIT(authentication_method, std::optional<std::string>);
DEF_PROPERTY_TRAIT(authentication_data, std::optional<std::string>);
DEF_PROPERTY_TRAIT(request_problem_information, std::optional<uint8_t>);
DEF_PROPERTY_TRAIT(will_delay_interval, std::optional<uint32_t>);
DEF_PROPERTY_TRAIT(request_response_information, std::optional<uint8_t>);
DEF_PROPERTY_TRAIT(response_information, std::optional<std::string>);
DEF_PROPERTY_TRAIT(server_reference, std::optional<std::string>);
DEF_PROPERTY_TRAIT(reason_string, std::optional<std::string>);
DEF_PROPERTY_TRAIT(receive_maximum, std::optional<uint16_t>);
DEF_PROPERTY_TRAIT(topic_alias_maximum, std::optional<uint16_t>);
DEF_PROPERTY_TRAIT(topic_alias, std::optional<uint16_t>);
DEF_PROPERTY_TRAIT(maximum_qos, std::optional<uint8_t>);
DEF_PROPERTY_TRAIT(retain_available, std::optional<uint8_t>);
DEF_PROPERTY_TRAIT(user_property, user_property_value_t);
DEF_PROPERTY_TRAIT(maximum_packet_size, std::optional<uint32_t>);
DEF_PROPERTY_TRAIT(wildcard_subscription_available, std::optional<uint8_t>);
DEF_PROPERTY_TRAIT(subscription_identifier_available, std::optional<uint8_t>);
DEF_PROPERTY_TRAIT(shared_subscription_available, std::optional<uint8_t>);

#undef DEF_PROPERTY_TRAIT

template <property_type p>
using value_type_t = typename property_traits<p>::type;

template <property_type p>
constexpr std::string_view name_v = property_traits<p>::name;

/**
 * \brief Base class for different Property classes that contains common access
        and modification functions.
 */
template <property_type ...Ps>
class properties {

    template <property_type p>
    struct property {
        using key = std::integral_constant<property_type, p>;
        constexpr static std::string_view name = name_v<p>;
        value_type_t<p> value;
    };
    std::tuple<property<Ps>...> _props;

public:

    /// \brief Accesses the value of a Property by its compile-time constant ID.
    template <property_type v>
    constexpr auto& operator[](std::integral_constant<property_type, v>)
    noexcept {
        return std::get<property<v>>(_props).value;
    }

    /// \copydoc operator[]
    template <property_type v>
    constexpr const auto& operator[](std::integral_constant<property_type, v>)
    const noexcept {
        return std::get<property<v>>(_props).value;
    }

/// \cond internal
    template <typename Func>
    using is_apply_on = std::conjunction<
        std::is_invocable<Func, value_type_t<Ps>&>...
    >;

    template <typename Func>
    using is_nothrow_apply_on = std::conjunction<
        std::is_nothrow_invocable<Func, value_type_t<Ps>&>...
    >;
/// \endcond

    /**
    * \brief Applies a function to a Property identified by its ID.
    *
    * This function iterates over all Properties and invokes the provided function
    * on the value of the Property that matches the given property ID.
    *
    * \param property_id The ID of the Property to apply the function on.
    * \param func The function to invoke on the Property value.
    * \return `true` if the function was applied to a Property and `false` otherwise.
    */
    template <
        typename Func,
        std::enable_if_t<is_apply_on<Func>::value, bool> = true
    >
    constexpr bool apply_on(uint8_t property_id, Func&& func)
    noexcept (is_nothrow_apply_on<Func>::value) {
        return std::apply(
            [&func, property_id](auto&... ptype) {
                auto pc = [&func, property_id](auto& px) {
                    using ptype = std::remove_reference_t<decltype(px)>;
                    constexpr typename ptype::key prop;
                    if (prop.value == property_id)
                        std::invoke(func, px.value);
                    return prop.value == property_id;
                };
                return (pc(ptype) || ...);
            },
            _props
        );
    }

/// \cond internal
    template <typename Func>
    using is_visitor = std::conjunction<
        std::is_invocable_r<bool, Func, decltype(Ps), value_type_t<Ps>&>...
    >;

    template <typename Func>
    using is_nothrow_visitor = std::conjunction<
        std::is_nothrow_invocable<Func, decltype(Ps), value_type_t<Ps>&>...
    >;
/// \endcond

    /**
    * \brief Invokes a visitor function on each Property.
    *
    * This function iterates over Properties and invokes the provided function 
    * on each Property, passing both the compile-time constant Property ID and its value.
    * The visitor function should return a boolean value that determines whether iteration continues.
    *
    * @param func The visitor function to invoke. It must accept two arguments: 
    *        a Property ID and the corresponding Property value.
    * @return `true` if the visitor function returns `true` for all Properties, 
    *         `false` otherwise.
    */
    template <
        typename Func,
        std::enable_if_t<is_visitor<Func>::value, bool> = true
    >
    constexpr bool visit(Func&& func)
    const noexcept (is_nothrow_visitor<Func>::value) {
        return std::apply(
            [&func](const auto&... props) {
                auto pc = [&func](const auto& px) {
                    using ptype = std::remove_reference_t<decltype(px)>;
                    constexpr typename ptype::key prop;
                    return std::invoke(func, prop, px.value);
                };
                return (pc(props) && ...);
            },
            _props
        );
    }

    /// \copydoc visit
    template <
        typename Func,
        std::enable_if_t<is_visitor<Func>::value, bool> = true
    >
    constexpr bool visit(Func&& func)
    noexcept (is_nothrow_visitor<Func>::value) {
        return std::apply(
            [&func](auto&... props) {
                auto pc = [&func](auto& px) {
                    using ptype = std::remove_reference_t<decltype(px)>;
                    constexpr typename ptype::key prop;
                    return std::invoke(func, prop, px.value);
                };
                return (pc(props) && ...);
            },
            _props
        );
    }
};


} // end namespace boost::mqtt5::prop

#endif // !BOOST_MQTT5_PROPERTY_TYPES_HPP
