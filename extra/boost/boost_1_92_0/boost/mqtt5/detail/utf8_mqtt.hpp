//
// Copyright (c) 2023-2026 Ivica Siladic, Bruno Iljazovic, Korina Simicevic
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MQTT5_UTF8_MQTT_HPP
#define BOOST_MQTT5_UTF8_MQTT_HPP

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

namespace boost::mqtt5::detail {

enum class validation_result : uint8_t {
    valid = 0,
    has_wildcard_character,
    invalid
};

inline int pop_front_unichar(std::string_view& s) {
    // assuming that s.length() is > 0

    int n = s[0] & 0xF0;
    int ch = -1;

    if ((n & 0x80) == 0) {
        ch = s[0];
        s.remove_prefix(1);
    }
    else if ((n == 0xC0 || n == 0xD0) && s.size() > 1) {
        ch = ((s[0] & 0x1F) << 6) | (s[1] & 0x3F);
        s.remove_prefix(2);
    }
    else if ((n == 0xE0) && s.size() > 2) {
        ch = ((s[0] & 0x1F) << 12) | ((s[1] & 0x3F) << 6) | (s[2] & 0x3F);
        s.remove_prefix(3);
    }
    else if ((n == 0xF0) && s.size() > 3) {
        ch = ((s[0] & 0x1F) << 18) | ((s[1] & 0x3F) << 12) |
            ((s[2] & 0x3F) << 6) | (s[3] & 0x3F);
        s.remove_prefix(4);
    }

    return ch;
}

inline validation_result validate_mqtt_utf8_char(int c) {
    constexpr int fe_flag = 0xFE;
    constexpr int ff_flag = 0xFF;

    constexpr int multi_lvl_wildcard = '#';
    constexpr int single_lvl_wildcard = '+';

    if (c == multi_lvl_wildcard || c == single_lvl_wildcard)
        return validation_result::has_wildcard_character;

    if (c > 0x001F && // U+0000...U+001F control characters
        (c < 0x007F || c > 0x009F) && // U+007F...0+009F control characters
        (c < 0xD800 || c > 0xDFFF) && // U+D800...U+DFFF surrogates
        (c < 0xFDD0 || c > 0xFDEF) && // U+FDD0...U+FDEF non-characters
        (c & fe_flag) != fe_flag && // non-characters
        (c & ff_flag) != ff_flag
    )
        return validation_result::valid;

    return validation_result::invalid;
}

inline bool is_valid_string_size(size_t sz) {
    constexpr size_t max_sz = 65535;
    return sz <= max_sz;
}

inline bool is_utf8(validation_result result) {
    return result == validation_result::valid ||
        result == validation_result::has_wildcard_character;
}

template <typename ValidSizeCondition, typename ValidCondition>
validation_result validate_impl(
    std::string_view str,
    ValidSizeCondition&& size_condition, ValidCondition&& condition
) {
    if (!size_condition(str.size()))
        return validation_result::invalid;

    validation_result result;
    while (!str.empty()) {
        int c = pop_front_unichar(str);

        result = validate_mqtt_utf8_char(c);
        if (!condition(result))
            return result;
    }

    return validation_result::valid;
}

inline validation_result validate_mqtt_utf8(std::string_view str) {
    return validate_impl(str, is_valid_string_size, is_utf8);
}

inline bool is_valid_string_pair(
    const std::pair<std::string, std::string>& str_pair
) {
    return validate_mqtt_utf8(str_pair.first) == validation_result::valid &&
        validate_mqtt_utf8(str_pair.second) == validation_result::valid;
}

} // namespace boost::mqtt5::detail

#endif //BOOST_MQTT5_UTF8_MQTT_HPP
