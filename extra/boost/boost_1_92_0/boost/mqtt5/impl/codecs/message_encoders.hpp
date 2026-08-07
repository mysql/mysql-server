//
// Copyright (c) 2023-2026 Ivica Siladic, Bruno Iljazovic, Korina Simicevic
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MQTT5_MESSAGE_ENCODERS_HPP
#define BOOST_MQTT5_MESSAGE_ENCODERS_HPP

#include <boost/mqtt5/types.hpp>

#include <boost/mqtt5/impl/codecs/base_encoders.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace boost::mqtt5::encoders {

template <typename encoder>
std::string encode(const encoder& e) {
    std::string s;
    s.reserve(e.byte_size());
    s << e;
    return s;
}

inline std::string encode_connect(
    std::string_view client_id,
    std::optional<std::string_view> user_name,
    std::optional<std::string_view> password,
    uint16_t keep_alive, bool clean_start,
    const connect_props& props,
    const std::optional<will>& w
) {

    auto packet_type_ =
        basic::flag<4>(0b0001) |
        basic::flag<4>(0);

    auto conn_flags_ =
        basic::flag<1>(user_name) |
        basic::flag<1>(password) |
        basic::flag<1>(w, &will::retain) |
        basic::flag<2>(w, &will::qos) |
        basic::flag<1>(w) |
        basic::flag<1>(clean_start) |
        basic::flag<1>(0);

    auto var_header_ =
        basic::utf8_("MQTT") &
        basic::byte_(uint8_t(5)) &
        conn_flags_ &
        basic::int16_(keep_alive) &
        prop::props_(props);

    auto payload_ =
        basic::utf8_(client_id) &
        prop::props_(w) &
        basic::utf8_(w, &will::topic) &
        basic::binary_(w, &will::message) &
        basic::utf8_(user_name) &
        basic::utf8_(password);

    auto message_body_ = var_header_ & payload_;

    auto fixed_header_ =
        packet_type_ &
        basic::varlen_(message_body_.byte_size());

    auto connect_message_ = fixed_header_ & message_body_;

    return encode(connect_message_);
}

inline std::string encode_connack(
    bool session_present,
    uint8_t reason_code,
    const connack_props& props
) {

    auto packet_type_ =
        basic::flag<4>(0b0010) |
        basic::flag<4>(0);

    auto var_header_ =
        basic::flag<1>(session_present) &
        basic::byte_(reason_code) &
        prop::props_(props);

    auto fixed_header_ =
        packet_type_ &
        basic::varlen_(var_header_.byte_size());

    auto connack_message_ = fixed_header_ & var_header_;

    return encode(connack_message_);
}

inline std::string encode_publish(
    uint16_t packet_id,
    std::string_view topic_name,
    std::string_view payload,
    qos_e qos, retain_e retain, dup_e dup,
    const publish_props& props
) {

    std::optional<uint16_t> used_packet_id;
    if (qos != qos_e::at_most_once) used_packet_id.emplace(packet_id);

    auto packet_type_ =
        basic::flag<4>(0b0011) |
        basic::flag<1>(dup) |
        basic::flag<2>(qos) |
        basic::flag<1>(retain);

    auto var_header_ =
        basic::utf8_(topic_name) &
        basic::int16_(used_packet_id) &
        prop::props_(props);

    auto message_body_ = var_header_ & basic::verbatim_(payload);

    auto fixed_header_ =
        packet_type_ &
        basic::varlen_(message_body_.byte_size());

    auto publish_message_ = fixed_header_ & message_body_;

    return encode(publish_message_);
}

inline std::string encode_puback(
    uint16_t packet_id,
    uint8_t reason_code,
    const puback_props& props
) {

    auto packet_type_ =
        basic::flag<4>(0b0100) |
        basic::flag<4>(0);

    auto var_header_ =
        basic::int16_(packet_id) &
        basic::byte_(reason_code) &
        prop::props_may_omit_(props);

    auto fixed_header_ =
        packet_type_ &
        basic::varlen_(var_header_.byte_size());

    auto puback_message_ = fixed_header_ & var_header_;

    return encode(puback_message_);
}

inline std::string encode_pubrec(
    uint16_t packet_id,
    uint8_t reason_code,
    const pubrec_props& props
) {

    auto packet_type_ =
        basic::flag<4>(0b0101) |
        basic::flag<4>(0b0000);

    auto var_header_ =
        basic::int16_(packet_id) &
        basic::byte_(reason_code) &
        prop::props_may_omit_(props);

    auto fixed_header_ =
        packet_type_ &
        basic::varlen_(var_header_.byte_size());

    auto pubrec_message_ = fixed_header_ & var_header_;

    return encode(pubrec_message_);
}

inline std::string encode_pubrel(
    uint16_t packet_id,
    uint8_t reason_code,
    const pubrel_props& props
) {

    auto packet_type_ =
        basic::flag<4>(0b0110) |
        basic::flag<4>(0b0010);

    auto var_header_ =
        basic::int16_(packet_id) &
        basic::byte_(reason_code) &
        prop::props_may_omit_(props);

    auto fixed_header_ =
        packet_type_ &
        basic::varlen_(var_header_.byte_size());

    auto pubrel_message_ = fixed_header_ & var_header_;

    return encode(pubrel_message_);
}

inline std::string encode_pubcomp(
    uint16_t packet_id,
    uint8_t reason_code,
    const pubcomp_props& props
) {

    auto packet_type_ =
        basic::flag<4>(0b0111) |
        basic::flag<4>(0b0000);

    auto var_header_ =
        basic::int16_(packet_id) &
        basic::byte_(reason_code) &
        prop::props_may_omit_(props);

    auto fixed_header_ =
        packet_type_ &
        basic::varlen_(var_header_.byte_size());

    auto pubcomp_message_ = fixed_header_ & var_header_;

    return encode(pubcomp_message_);
}

inline std::string encode_subscribe(
    uint16_t packet_id,
    const std::vector<subscribe_topic>& topics,
    const subscribe_props& props
) {

    auto packet_type_ =
        basic::flag<4>(0b1000) |
        basic::flag<4>(0b0010);

    size_t payload_size = 0;
    for (const auto& [topic_filter, _]: topics)
        payload_size += basic::utf8_(topic_filter).byte_size() + 1;

    auto var_header_ =
        basic::int16_(packet_id) &
        prop::props_(props);

    auto message_ =
        packet_type_ &
        basic::varlen_(var_header_.byte_size()  + payload_size) &
        var_header_;

    auto s = encode(message_);
    s.reserve(s.size() + payload_size);

    for (const auto& [topic_filter, sub_opts]: topics) {
        auto opts_ =
            basic::flag<2>(sub_opts.retain_handling) |
            basic::flag<1>(sub_opts.retain_as_published) |
            basic::flag<1>(sub_opts.no_local) |
            basic::flag<2>(sub_opts.max_qos);
        auto filter_ = basic::utf8_(topic_filter) & opts_;
        s << filter_;
    }

    return s;
}

inline std::string encode_suback(
    uint16_t packet_id,
    const std::vector<uint8_t>& reason_codes,
    const suback_props& props
) {

    auto packet_type_ =
        basic::flag<4>(0b1001) |
        basic::flag<4>(0b0000);

    auto var_header_ =
        basic::int16_(packet_id) &
        prop::props_(props);

    auto message_ =
        packet_type_ &
        basic::varlen_(var_header_.byte_size()  + reason_codes.size()) &
        var_header_;

    auto s = encode(message_);
    s.reserve(s.size() + reason_codes.size());

    for (auto reason_code: reason_codes)
        s << basic::byte_(reason_code);

    return s;
}

inline std::string encode_unsubscribe(
    uint16_t packet_id,
    const std::vector<std::string>& topics,
    const unsubscribe_props& props
) {

    auto packet_type_ =
        basic::flag<4>(0b1010) |
        basic::flag<4>(0b0010);

    size_t payload_size = 0;
    for (const auto& topic: topics)
        payload_size += basic::utf8_(topic).byte_size();

    auto var_header_ =
        basic::int16_(packet_id) &
        prop::props_(props);

    auto message_ =
        packet_type_ &
        basic::varlen_(var_header_.byte_size()  + payload_size) &
        var_header_;

    auto s = encode(message_);
    s.reserve(s.size() + payload_size);

    for (const auto& topic: topics)
        s << basic::utf8_(topic);

    return s;
}

inline std::string encode_unsuback(
    uint16_t packet_id,
    const std::vector<uint8_t>& reason_codes,
    const unsuback_props& props
) {

    auto packet_type_ =
        basic::flag<4>(0b1011) |
        basic::flag<4>(0b0000);

    auto var_header_ =
        basic::int16_(packet_id) &
        prop::props_(props);

    auto message_ =
        packet_type_ &
        basic::varlen_(var_header_.byte_size()  + reason_codes.size()) &
        var_header_;

    auto s = encode(message_);
    s.reserve(s.size() + reason_codes.size());

    for (auto reason_code: reason_codes)
        s << basic::byte_(reason_code);

    return s;
}

inline std::string encode_pingreq() {
    auto packet_type_ =
        basic::flag<4>(0b1100) |
        basic::flag<4>(0);

    auto remaining_len_ =
        basic::byte_(uint8_t(0));

    auto ping_req_ = packet_type_ & remaining_len_;

    return encode(ping_req_);
}

inline std::string encode_pingresp() {
    auto packet_type_ =
        basic::flag<4>(0b1101) |
        basic::flag<4>(0);

    auto remaining_len_ =
        basic::byte_(uint8_t(0));

    auto ping_resp_ = packet_type_ & remaining_len_;

    return encode(ping_resp_);
}

inline std::string encode_disconnect(
    uint8_t reason_code,
    const disconnect_props& props
) {

    auto packet_type_ =
        basic::flag<4>(0b1110) |
        basic::flag<4>(0b0000);

    auto var_header_ =
        basic::byte_(reason_code) &
        prop::props_(props);

    auto fixed_header_ =
        packet_type_ &
        basic::varlen_(var_header_.byte_size());

    auto disconnect_message_ = fixed_header_ & var_header_;

    return encode(disconnect_message_);
}

inline std::string encode_auth(
    uint8_t reason_code,
    const auth_props& props
) {

    auto packet_type_ =
        basic::flag<4>(0b1111) |
        basic::flag<4>(0b0000);

    auto var_header_ =
        basic::byte_(reason_code) &
        prop::props_(props);

    auto fixed_header_ =
        packet_type_ &
        basic::varlen_(var_header_.byte_size());

    auto auth_message_ = fixed_header_ & var_header_;

    return encode(auth_message_);
}


} // end namespace boost::mqtt5::encoders

#endif // !BOOST_MQTT5_MESSAGE_ENCODERS_HPP
