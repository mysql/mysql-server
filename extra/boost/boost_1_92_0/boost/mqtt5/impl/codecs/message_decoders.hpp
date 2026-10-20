//
// Copyright (c) 2023-2026 Ivica Siladic, Bruno Iljazovic, Korina Simicevic
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MQTT5_MESSAGE_DECODERS_HPP
#define BOOST_MQTT5_MESSAGE_DECODERS_HPP

#include <boost/mqtt5/types.hpp>

#include <boost/mqtt5/impl/codecs/base_decoders.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace boost::mqtt5::decoders {

using byte_citer = std::string::const_iterator;

using fixed_header = std::tuple<
    uint8_t, // control byte
    uint32_t // remaining_length
>;

inline std::optional<fixed_header> decode_fixed_header(
    byte_citer& it, const byte_citer last
) {
    constexpr auto fixed_header_ = basic::byte_ >> basic::varint_;
    return type_parse(it, last, fixed_header_);
}

using packet_id = uint16_t;

inline std::optional<packet_id> decode_packet_id(
    byte_citer& it, const byte_citer last
) {
    constexpr auto packet_id_ = basic::word_;
    return type_parse(it, last, packet_id_);
}

using connect_message = std::tuple<
    std::string, // client_id,
    std::optional<std::string>, // user_name,
    std::optional<std::string>, // password,
    uint16_t, // keep_alive,
    bool, // clean_start,
    connect_props, // props,
    std::optional<will> // will
>;

inline std::optional<connect_message> decode_connect(
    uint32_t remain_length, byte_citer& it
) {
    constexpr auto var_header_ =
        basic::utf8_ >> // MQTT
        basic::byte_ >> // (num 5)
        basic::byte_ >> // conn_flags_
        basic::word_ >> // keep_alive
        prop::props_<connect_props>;

    const byte_citer end = it + remain_length;
    auto vh = type_parse(it, end, var_header_);
    if (!vh)
        return std::optional<connect_message>{};

    auto& [mqtt_str, version, flags, keep_alive, cprops] = *vh;

    if (mqtt_str != "MQTT" || version != 5)
        return std::optional<connect_message>{};

    bool has_will =  (flags & 0b00000100);
    bool has_uname = (flags & 0b10000000);
    bool has_pwd =   (flags & 0b01000000);

    auto payload_ =
        basic::utf8_ >> // client_id
        basic::if_(has_will)[prop::props_<will_props>] >>
        basic::if_(has_will)[basic::utf8_] >> // will topic
        basic::if_(has_will)[basic::binary_] >> // will message
        basic::if_(has_uname)[basic::utf8_] >> // username
        basic::if_(has_pwd)[basic::utf8_]; // password

    auto pload = type_parse(it, end, payload_);
    if (!pload)
        return std::optional<connect_message>{};

    std::optional<will> w;

    if (has_will)
        w.emplace(
            std::move(*std::get<2>(*pload)), // will_topic
            std::move(*std::get<3>(*pload)), // will_message
            qos_e((flags & 0b00011000) >> 3),
            retain_e((flags & 0b00100000) >> 5),
            std::move(*std::get<1>(*pload)) // will props
        );

    connect_message retval = {
        std::move(std::get<0>(*pload)), // client_id
        std::move(std::get<4>(*pload)), // user_name
        std::move(std::get<5>(*pload)), // password
        keep_alive,
        flags & 0b00000010, // clean_start
        std::move(cprops), // connect_props
        std::move(w) // will
    };

    return std::optional<connect_message> { std::move(retval) };
}

using connack_message = std::tuple<
    uint8_t, // session_present
    uint8_t, // connect reason code
    connack_props // props
>;

inline std::optional<connack_message> decode_connack(
    uint32_t remain_length, byte_citer& it
) {
    constexpr auto connack_ = basic::byte_ >> basic::byte_ >> prop::props_<connack_props>;
    return type_parse(it, it + remain_length, connack_);
}

using publish_message = std::tuple<
    std::string, // topic
    std::optional<uint16_t>, // packet_id
    uint8_t, // dup_e, qos_e, retain_e
    publish_props, // publish props
    std::string // payload
>;

inline std::optional<publish_message> decode_publish(
    uint8_t control_byte, uint32_t remain_length, byte_citer& it
) {
    uint8_t flags = control_byte & 0b1111;
    auto qos = qos_e((flags >> 1) & 0b11);

    auto publish_ =
        basic::utf8_ >> basic::if_(qos != qos_e::at_most_once)[basic::word_] >>
            basic::attr(flags) >> prop::props_<publish_props> >>
            basic::verbatim_;
    return type_parse(it, it + remain_length, publish_);
}

using puback_message = std::tuple<
    uint8_t, // puback reason code
    puback_props // props
>;

inline std::optional<puback_message> decode_puback(
    uint32_t remain_length, byte_citer& it
) {
    if (remain_length == 0)
        return puback_message {};
    constexpr auto puback_ = basic::byte_ >> prop::props_<puback_props>;
    return type_parse(it, it + remain_length, puback_);
}

using pubrec_message = std::tuple<
    uint8_t, // puback reason code
    pubrec_props // props
>;

inline std::optional<pubrec_message> decode_pubrec(
    uint32_t remain_length, byte_citer& it
) {
    if (remain_length == 0)
        return pubrec_message {};
    constexpr auto pubrec_ = basic::byte_ >> prop::props_<pubrec_props>;
    return type_parse(it, it + remain_length, pubrec_);
}

using pubrel_message = std::tuple<
    uint8_t, // puback reason code
    pubrel_props // props
>;

inline std::optional<pubrel_message> decode_pubrel(
    uint32_t remain_length, byte_citer& it
) {
    if (remain_length == 0)
        return pubrel_message {};
    constexpr auto pubrel_ = basic::byte_ >> prop::props_<pubrel_props>;
    return type_parse(it, it + remain_length, pubrel_);
}

using pubcomp_message = std::tuple<
    uint8_t, // puback reason code
    pubcomp_props // props
>;

inline std::optional<pubcomp_message> decode_pubcomp(
    uint32_t remain_length, byte_citer& it
) {
    if (remain_length == 0)
        return pubcomp_message {};
    constexpr auto pubcomp_ = basic::byte_ >> prop::props_<pubcomp_props>;
    return type_parse(it, it + remain_length, pubcomp_);
}

using subscribe_message = std::tuple<
    subscribe_props,
    std::vector<std::tuple<std::string, uint8_t>> // topic filter with opts
>;

inline std::optional<subscribe_message> decode_subscribe(
    uint32_t remain_length, byte_citer& it
) {
    constexpr auto subscribe_ = prop::props_<subscribe_props> >> +(basic::utf8_ >> basic::byte_);
    return type_parse(it, it + remain_length, subscribe_);
}

using suback_message = std::tuple<
    suback_props,
    std::vector<uint8_t> // reason_codes
>;

inline std::optional<suback_message> decode_suback(
    uint32_t remain_length, byte_citer& it
) {
    constexpr auto suback_ = prop::props_<suback_props> >> +basic::byte_;
    return type_parse(it, it + remain_length, suback_);
}

using unsubscribe_message = std::tuple<
    unsubscribe_props,
    std::vector<std::string> // topics
>;

inline std::optional<unsubscribe_message> decode_unsubscribe(
    uint32_t remain_length, byte_citer& it
) {
    constexpr auto unsubscribe_ = prop::props_<unsubscribe_props> >> +basic::utf8_;
    return type_parse(it, it + remain_length, unsubscribe_);
}

using unsuback_message = std::tuple<
    unsuback_props,
    std::vector<uint8_t> // reason_codes
>;

inline std::optional<unsuback_message> decode_unsuback(
    uint32_t remain_length, byte_citer& it
) {
    constexpr auto unsuback_ = prop::props_<unsuback_props> >> +basic::byte_;
    return type_parse(it, it + remain_length, unsuback_);
}

using disconnect_message = std::tuple<
    uint8_t, // reason_code
    disconnect_props
>;

inline std::optional<disconnect_message> decode_disconnect(
    uint32_t remain_length, byte_citer& it
) {
    if (remain_length == 0)
        return disconnect_message {};
    constexpr auto disconnect_ = basic::byte_ >> prop::props_<disconnect_props>;
    return type_parse(it, it + remain_length, disconnect_);
}

using auth_message = std::tuple<
    uint8_t, // reason_code
    auth_props
>;

inline std::optional<auth_message> decode_auth(
    uint32_t remain_length, byte_citer& it
) {
    if (remain_length == 0)
        return auth_message {};
    constexpr auto auth_ = basic::byte_ >> prop::props_<auth_props>;
    return type_parse(it, it + remain_length, auth_);
}


} // end namespace boost::mqtt5::decoders

#endif // !BOOST_MQTT5_MESSAGE_DECODERS_HPP
