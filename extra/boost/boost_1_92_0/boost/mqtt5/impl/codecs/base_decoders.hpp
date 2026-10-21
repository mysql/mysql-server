//
// Copyright (c) 2023-2026 Ivica Siladic, Bruno Iljazovic, Korina Simicevic
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MQTT5_BASE_DECODERS_HPP
#define BOOST_MQTT5_BASE_DECODERS_HPP

#include <boost/mqtt5/property_types.hpp>

#include <boost/mqtt5/detail/traits.hpp>

#include <boost/endian/conversion.hpp>

#include <cstdint>
#include <string>
#include <utility>

namespace boost::mqtt5::decoders {

template <typename It, typename Parser>
auto type_parse(It& first, const It last, const Parser& p) {
    using rv_type = typename Parser::attribute_type;

    std::optional<rv_type> rv;
    rv_type value {};
    if (p.parse(first, last, value))
        rv = std::move(value);
    return rv;
}

namespace basic {

template <typename Attr>
struct int_parser {
    using attribute_type = Attr;

    template <typename It>
    bool parse(It& first, const It last, Attr& attr) const {
        constexpr size_t byte_size = sizeof(Attr);
        if (std::distance(first, last) < static_cast<ptrdiff_t>(byte_size))
            return false;

        using namespace boost::endian;
        attr = endian_load<Attr, sizeof(Attr), order::big>(
            reinterpret_cast<const uint8_t*>(static_cast<const char*>(&*first))
        );
        first += sizeof(Attr);

        return true;
    }
};

inline constexpr int_parser<uint8_t> byte_;
inline constexpr int_parser<uint16_t> word_;
inline constexpr int_parser<uint32_t> dword_;

struct varint_parser {
    using attribute_type = int32_t;

    template <typename It>
    bool parse(It& first, const It last, int32_t& attr) const {
        auto iter = first;

        if (iter == last)
            return false;

        int32_t result = 0; unsigned bit_shift = 0;

        for (; iter != last && bit_shift < sizeof(int32_t) * 7; ++iter) {
            auto val = *iter;
            if (val & 0b1000'0000u) {
                result |= (val & 0b0111'1111u) << bit_shift;
                bit_shift += 7;
            }
            else {
                result |= (static_cast<int32_t>(val) << bit_shift);
                bit_shift = 0;
                break;
            }
        }
        if (bit_shift)
            return false;

        attr = result;
        first = ++iter;
        return true;
    }
};

inline constexpr varint_parser varint_;

struct len_prefix_parser {
    using attribute_type = std::string;

    template <typename It>
    bool parse(It& first, const It last, std::string& attr) const {
        auto iter = first;

        typename decltype(word_)::attribute_type len;
        if (word_.parse(iter, last, len)) {
            if (std::distance(iter, last) < len)
                return false;
        }
        else
            return false;

        attr = std::string(iter, iter + len);
        first = iter + len;
        return true;
    }
};

inline constexpr len_prefix_parser utf8_ {};
inline constexpr len_prefix_parser binary_ {};

template <typename Subject>
struct conditional_parser {
    using subject_attr_type = typename Subject::attribute_type;
    using attribute_type = std::optional<subject_attr_type>;

    Subject p;
    bool condition;

    template <typename It>
    bool parse(It& first, const It last, attribute_type& attr) const {
        if (!condition)
            return true;

        auto iter = first;
        subject_attr_type sattr {};
        if (!p.parse(iter, last, sattr))
            return false;

        attr.emplace(std::move(sattr));
        first = iter;
        return true;
    }
};

struct conditional_gen {
    bool _condition;

    template <typename Subject>
    conditional_parser<Subject> operator[](const Subject& p) const {
        return { p, _condition };
    }
};

constexpr conditional_gen if_(bool condition) {
    return { condition };
}

struct verbatim_parser {
    using attribute_type = std::string;

    template <typename It>
    bool parse(It& first, const It last, std::string& attr) const {
        attr = std::string { first, last };
        first = last;
        return true;
    }
};

inline constexpr auto verbatim_ = verbatim_parser{};

template <typename... Ps>
struct seq_parser {
    using attribute_type = std::tuple<typename Ps::attribute_type...>;

    std::tuple<Ps...> parsers;

    template <typename It>
    bool parse(It& first, const It last, attribute_type& attr) const {
        return parse_impl(
            first, last, attr, std::make_index_sequence<sizeof...(Ps)>{}
        );
    }

private:
    template <typename It, size_t... Is>
    bool parse_impl(
        It& first, const It last, attribute_type& attr,
        std::index_sequence<Is...>
    ) const {
        auto iter = first;
        bool rv = (
            std::get<Is>(parsers).parse(iter, last, std::get<Is>(attr))
                && ...
        );
        if (rv)
            first = iter;
        return rv;
    }
};

template <typename... Ps, typename P2>
constexpr seq_parser<Ps..., P2> operator>>(
    const seq_parser<Ps...>& p1, const P2& p2
) {
    return { std::tuple_cat(p1.parsers, std::make_tuple(p2)) };
}

template <typename P1, typename P2>
constexpr seq_parser<P1, P2> operator>>(const P1& p1, const P2& p2) {
    return { std::make_tuple(p1, p2) };
}

template <typename Attr>
struct attr_parser {
    using attribute_type = Attr;

    Attr attr;

    template <typename It>
    bool parse(It&, const It, Attr& attr) const {
        attr = this->attr;
        return true;
    }
};

template <typename Attr>
constexpr attr_parser<Attr> attr(const Attr& val) {
    return { val };
}

template <typename Subject>
struct plus_parser {
    using subject_attr_type = typename Subject::attribute_type;
    using attribute_type = std::vector<subject_attr_type>;

    Subject p;

    template <typename It, typename Attr>
    bool parse(It& first, const It last, Attr& attr) const {
        bool success = false;
        while (first < last) {
            subject_attr_type sattr {};
            auto iter = first;
            if (!p.parse(iter, last, sattr)) break;

            success = true;
            first = iter;
            attr.push_back(std::move(sattr));
        }

        return success;
    }
};

template <typename P>
constexpr plus_parser<P> operator+(const P& p) {
    return { p };
}

} // end namespace basic

namespace prop {

namespace detail {

template <typename It, typename Prop>
bool parse_to_prop(It& iter, const It last, Prop& prop) {
    using namespace boost::mqtt5::detail;
    using prop_type = std::remove_reference_t<decltype(prop)>;

    bool rv = false;

    if constexpr (std::is_same_v<prop_type, uint8_t>)
        rv = basic::byte_.parse(iter, last, prop);
    else if constexpr (std::is_same_v<prop_type, uint16_t>)
        rv = basic::word_.parse(iter, last, prop);
    else if constexpr (std::is_same_v<prop_type, int32_t>)
        rv = basic::varint_.parse(iter, last, prop);
    else if constexpr (std::is_same_v<prop_type, uint32_t>)
        rv = basic::dword_.parse(iter, last, prop);
    else if constexpr (std::is_same_v<prop_type, std::string>)
        rv = basic::utf8_.parse(iter, last, prop);

    else if constexpr (is_optional<prop_type>) {
        typename prop_type::value_type val;
        rv = parse_to_prop(iter, last, val);
        if (rv) prop.emplace(std::move(val));
    }

    else if constexpr (is_pair<prop_type>) {
        rv = parse_to_prop(iter, last, prop.first);
        rv = parse_to_prop(iter, last, prop.second);
    }

    else if constexpr (is_vector<prop_type> || is_small_vector<prop_type>) {
        typename std::remove_reference_t<prop_type>::value_type value;
        rv = parse_to_prop(iter, last, value);
        if (rv) prop.push_back(std::move(value));
    }

    return rv;
}

} // end namespace detail

template <typename Props>
struct prop_parser {
    using attribute_type = Props;

    template <typename It>
    bool parse(It& first, const It last, Props& attr) const {
        auto iter = first;

        if (iter == last)
            return true;

        int32_t props_length;
        if (!basic::varint_.parse(iter, last, props_length))
            return false;

        if (props_length < 0 || props_length > std::distance(iter, last))
            return false;

        const auto scoped_last = iter + props_length;
        // attr = Props{};

        while (iter < scoped_last) {
            uint8_t prop_id = *iter++;
            bool rv = true;
            It saved = iter;

            attr.apply_on(
                prop_id,
                [&rv, &iter, scoped_last](auto& prop) {
                    rv = detail::parse_to_prop(iter, scoped_last, prop);
                }
            );

            // either rv = false or property with prop_id was not found
            if (!rv || iter == saved)
                return false;
        }

        first = iter;
        return true;
    }
};

template <typename Props>
constexpr auto props_ = prop_parser<Props> {};

} // end namespace prop

} // end namespace boost::mqtt5::decoders

#endif // !BOOST_MQTT5_BASE_DECODERS_HPP
