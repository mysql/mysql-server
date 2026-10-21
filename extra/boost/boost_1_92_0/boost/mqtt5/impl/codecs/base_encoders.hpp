//
// Copyright (c) 2023-2026 Ivica Siladic, Bruno Iljazovic, Korina Simicevic
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MQTT5_BASE_ENCODERS_HPP
#define BOOST_MQTT5_BASE_ENCODERS_HPP

#include <boost/mqtt5/property_types.hpp>

#include <boost/mqtt5/detail/traits.hpp>

#include <boost/core/identity.hpp>
#include <boost/endian/conversion.hpp>
#include <boost/type_traits/is_detected_exact.hpp>
#include <boost/type_traits/remove_cv_ref.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <type_traits>
#include <utility>

namespace boost::mqtt5::encoders {

namespace basic {

using varint_t = int*;

inline void to_variable_bytes(std::string& s, int32_t val) {
    if (val > 0xfffffff) return;
    while (val > 127) {
        s.push_back(char((val & 0b01111111) | 0b10000000));
        val >>= 7;
    }
    s.push_back(val & 0b01111111);
}

inline size_t variable_length(int32_t val) {
    if (val > 0xfffffff) return 0;
    size_t rv = 1;
    for (; val > 127; ++rv) val >>= 7;
    return rv;
}

struct encoder {};

template <size_t bits, typename repr = uint8_t>
class flag_def : public encoder {
    template <size_t num_bits>
    using least_type = std::conditional_t<
        num_bits <= 8, uint8_t,
        std::conditional_t<
            num_bits <= 16, uint16_t,
            std::conditional_t<
                num_bits <= 32, uint32_t,
                std::conditional_t<num_bits <= 64, uint64_t, void>
            >
        >
    >;

    template <size_t oth_bits, typename oth_repr>
    friend class flag_def;

    repr _val { 0 };

public:
    flag_def(repr val) : _val(val) {}
    flag_def() = default;

    template <
        typename T,
        typename projection = boost::identity,
        std::enable_if_t<detail::is_optional<T>, bool> = true
    >
    auto operator()(T&& value, projection proj = {}) const {
        if constexpr (std::is_same_v<projection, boost::identity>) {
            repr val = value.has_value();
            return flag_def<bits, repr> { val };
        }
        else {
            repr val = value.has_value() ?
                static_cast<repr>(std::invoke(proj, *value)) : 0;
            return flag_def<bits, repr> { val };
        }
    }

    template <
        typename T,
        typename projection = boost::identity,
        std::enable_if_t<!detail::is_optional<T>, bool> = true
    >
    auto operator()(T&& value, projection proj = {}) const {
        auto val = static_cast<repr>(std::invoke(proj, value));
        return flag_def<bits, repr> { val };
    }

    size_t byte_size() const { return sizeof(repr); }

    template <size_t rhs_bits, typename rhs_repr>
    auto operator|(const flag_def<rhs_bits, rhs_repr>& rhs) const {
        using res_repr = least_type<bits + rhs_bits>;
        auto val = static_cast<res_repr>((_val << rhs_bits) | rhs._val);
        return flag_def<bits + rhs_bits, res_repr> { val };
    }

    std::string& encode(std::string& s) const {
        using namespace boost::endian;
        size_t sz = s.size(); s.resize(sz + sizeof(repr));
        auto p = reinterpret_cast<uint8_t*>(s.data() + sz);
        endian_store<repr, sizeof(repr), order::big>(p, _val);
        return s;
    }
};

template <size_t bits, typename repr = uint8_t>
constexpr auto flag = flag_def<bits, repr>{};


template <typename T, typename Repr>
class int_val : public encoder {
    T _val;
public:
    int_val(T val) : _val(val) {}

    size_t byte_size() const {
        if constexpr (detail::is_optional<T>) {
            if (_val) return val_length(*_val);
            return 0;
        }
        else
            return val_length(_val);
    }

    std::string& encode(std::string& s) const {
        if constexpr (detail::is_optional<T>) {
            if (_val) return encode_val(s, *_val);
            return s;
        }
        else
            return encode_val(s, _val);
    }
private:
    template <typename U>
    static size_t val_length(U&& val) {
        if constexpr (std::is_same_v<Repr, varint_t>)
            return variable_length(int32_t(val));
        else
            return sizeof(Repr);
    }

    template <typename U>
    static std::string& encode_val(std::string& s, U&& val) {
        using namespace boost::endian;
        if constexpr (std::is_same_v<Repr, varint_t>) {
            to_variable_bytes(s, int32_t(val));
            return s;
        }
        else {
            size_t sz = s.size(); s.resize(sz + sizeof(Repr));
            auto p = reinterpret_cast<uint8_t*>(s.data() + sz);
            endian_store<Repr, sizeof(Repr), order::big>(p, val);
            return s;
        }
    }
};

template <typename Repr>
class int_def {
public:
    template <typename T>
    auto operator()(T&& val) const {
        return int_val<T, Repr> { std::forward<T>(val) };
    }

    template <typename T, typename projection>
    auto operator()(T&& val, projection proj) const {
        if constexpr (detail::is_optional<T>) {
            using rv_type = std::invoke_result_t<
                projection, typename boost::remove_cv_ref_t<T>::value_type
            >;
            if (val.has_value())
                return (*this)(std::invoke(proj, *val));
            return int_val<rv_type, Repr> { rv_type {} };
        }
        else {
            using rv_type = std::invoke_result_t<projection, T>;
            return int_val<rv_type, Repr> { std::invoke(proj, val) };
        }
    }
};

constexpr auto byte_ = int_def<uint8_t> {};
constexpr auto int16_ = int_def<uint16_t> {};
constexpr auto int32_ = int_def<uint32_t> {};
constexpr auto varlen_ = int_def<varint_t> {};


template <typename T>
class array_val : public encoder {
    T _val;
    bool _with_length;
public:
    array_val(T val, bool with_length) :
        _val(val), _with_length(with_length)
    {
        static_assert(
            std::is_reference_v<T> || std::is_same_v<T, std::string_view>
        );
    }

    size_t byte_size() const {
        if constexpr (detail::is_optional<T>)
            return _val ? _with_length * 2 + val_length(*_val) : 0;
        else
            return _with_length * 2 + val_length(_val);
    }

    std::string& encode(std::string& s) const {
        if constexpr (detail::is_optional<T>) {
            if (_val) return encode_val(s, *_val);
            return s;
        }
        else
            return encode_val(s, _val);
    }

private:
    template <typename V>
    using has_size = decltype(std::declval<V&>().size());

    template <typename U>
    static size_t val_length(U&& val) {
        if constexpr (std::is_same_v<boost::remove_cv_ref_t<U>, const char*>)
            return std::strlen(val);

        if constexpr (boost::is_detected_exact_v<size_t, has_size, U>)
            return val.size();
        else // fallback to type const char (&)[N] (substract 1 for trailing 0)
            return sizeof(val) - 1;
    }

    template <typename U>
    std::string& encode_val(std::string& s, U&& u) const {
        using namespace boost::endian;
        auto byte_len = val_length(std::forward<U>(u));
        if (byte_len == 0 && !_with_length) return s;
        if (_with_length) {
            size_t sz = s.size(); s.resize(sz + 2);
            auto p = reinterpret_cast<uint8_t*>(s.data() + sz);
            endian_store<int16_t, sizeof(int16_t), order::big>(
                p, int16_t(byte_len)
            );
        }
        s.append(std::begin(u), std::begin(u) + byte_len);
        return s;
    }
};

template <bool with_length = true>
class array_def {
public:
    template <typename T>
    auto operator()(T&& val) const {
        return array_val<T> { std::forward<T>(val), with_length };
    }

    template <typename T, typename projection>
    auto operator()(T&& val, projection proj) const {
        if constexpr (detail::is_optional<T>) {
            using rv_type = std::invoke_result_t<
                projection, typename boost::remove_cv_ref_t<T>::value_type
            >;
            if (val.has_value())
                return (*this)(std::invoke(proj, *val));
            return array_val<rv_type> { rv_type {}, false };
        }
        else {
            const auto& av = std::invoke(proj, val);
            return array_val<T> { av, true };
        }
    }
};

using utf8_def = array_def<true>;

constexpr auto utf8_ = utf8_def {};
constexpr auto binary_ = array_def<true> {}; // for now
constexpr auto verbatim_ = array_def<false> {};


template <typename T, typename U>
class composed_val : public encoder {
    T _lhs; U _rhs;
public:
    composed_val(T lhs, U rhs) :
        _lhs(std::forward<T>(lhs)), _rhs(std::forward<U>(rhs))
    {}

    size_t byte_size() const {
        return _lhs.byte_size() + _rhs.byte_size();
    }

    std::string& encode(std::string& s) const {
        _lhs.encode(s);
        return _rhs.encode(s);
    }
};

template <
    typename T, typename U,
    std::enable_if_t<
        std::is_base_of_v<encoder, std::decay_t<T>> &&
        std::is_base_of_v<encoder, std::decay_t<U>>,
        bool
    > = true
>
inline auto operator&(T&& t, U&& u) {
    return composed_val(std::forward<T>(t), std::forward<U>(u));
}

template <
    typename T,
    std::enable_if_t<std::is_base_of_v<encoder, std::decay_t<T>>, bool> = true
>
std::string& operator<<(std::string& s, T&& t) {
    return t.encode(s);
}

} // end namespace basic

namespace prop {

namespace pp = boost::mqtt5::prop;

template <typename T>
auto encoder_for_prop_value(const T& val) {
    if constexpr (std::is_same_v<T, uint8_t>)
        return basic::int_def<uint8_t>{}(val);
    else if constexpr (std::is_same_v<T, uint16_t>)
        return basic::int_def<uint16_t>{}(val);
    else if constexpr (std::is_same_v<T, int32_t>)
        return basic::int_def<basic::varint_t>{}(val);
    else if constexpr (std::is_same_v<T, uint32_t>)
        return basic::int_def<uint32_t>{}(val);
    else if constexpr (std::is_same_v<T, std::string>)
        return basic::utf8_def{}(val);
    else if constexpr (detail::is_pair<T>)
        return encoder_for_prop_value(val.first) &
            encoder_for_prop_value(val.second);
}

template <typename T, pp::property_type p, typename Enable = void>
class prop_val;

template <
    typename T, pp::property_type p
>
class prop_val<
    T, p,
    std::enable_if_t<!detail::is_vector<T> && detail::is_optional<T>>
> : public basic::encoder {
    // allows T to be reference type to std::optional
    static inline boost::remove_cv_ref_t<T> nulltype;
    T _val;
public:
    prop_val(T val) : _val(val) {
        static_assert(std::is_reference_v<T>);
    }
    prop_val() : _val(nulltype) {}

    size_t byte_size() const {
        if (!_val) return 0;
        return 1 + encoder_for_prop_value(*_val).byte_size();
    }

    std::string& encode(std::string& s) const {
        if (!_val)
            return s;
        s.push_back(p);
        return encoder_for_prop_value(*_val).encode(s);
    }
};

template <
    typename T, pp::property_type p
>
class prop_val<
    T, p,
    std::enable_if_t<detail::is_vector<T> || detail::is_small_vector<T>>
> : public basic::encoder {
    // allows T to be reference type to std::vector
    static inline boost::remove_cv_ref_t<T> nulltype;
    T _val;
public:
    prop_val(T val) : _val(val) {
        static_assert(std::is_reference_v<T>);
    }

    prop_val() : _val(nulltype) { }

    size_t byte_size() const {
        if (_val.empty()) return 0;

        size_t total_size = 0;
        for (const auto& elem : _val)
            total_size += 1 + encoder_for_prop_value(elem).byte_size();

        return total_size;
    }

    std::string& encode(std::string& s) const {
        if (_val.empty())
            return s;

        for (const auto& elem: _val) {
            s.push_back(p);
            encoder_for_prop_value(elem).encode(s);
        }

        return s;
    }
};


template <typename Props>
class props_val : public basic::encoder {
    static inline std::decay_t<Props> nulltype;

    template <pp::property_type P, typename T>
    static auto to_prop_val(const T& val) {
        return prop_val<const T&, P>(val);
    }

    template <pp::property_type ...Ps>
    static auto to_prop_vals(const pp::properties<Ps...>& props) {
        return std::make_tuple(
            to_prop_val<Ps>(
                props[std::integral_constant<pp::property_type, Ps> {}]
            )...
        );
    }

    template <typename Func>
    auto apply_each(Func&& func) const {
        return std::apply([&func](const auto&... props) {
            return (std::invoke(func, props), ...);
        }, _prop_vals);
    }

    decltype(to_prop_vals(std::declval<Props>())) _prop_vals;
    bool _may_omit;

public:
    props_val(Props val, bool may_omit) :
        _prop_vals(to_prop_vals(val)), _may_omit(may_omit)
    {
        static_assert(std::is_reference_v<Props>);
    }
    props_val(bool may_omit) :
        _prop_vals(to_prop_vals(nulltype)), _may_omit(may_omit)
    {}

    size_t byte_size() const {
        size_t psize = props_size();
        if (_may_omit && psize == 0) return 0;
        return psize + basic::varlen_(psize).byte_size();
    }

    std::string& encode(std::string& s) const {
        size_t psize = props_size();
        if (_may_omit && psize == 0) return s;
        basic::varlen_(psize).encode(s);
        apply_each([&s](const auto& pv) { return pv.encode(s); });
        return s;
    }
private:
    size_t props_size() const {
        size_t retval = 0;
        apply_each([&retval](const auto& pv) {
            return retval += pv.byte_size();
        });
        return retval;
    }
};

template <bool may_omit>
class props_def {
public:
    template <typename T>
    auto operator()(T&& prop_container) const {
        if constexpr (detail::is_optional<T>) {
            if (prop_container.has_value())
                return (*this)(*prop_container);
            return props_val<
                const typename boost::remove_cv_ref_t<T>::value_type&
            >(true);
        }
        else {
            return props_val<T> { prop_container, may_omit };
        }
    }
};

constexpr auto props_ = props_def<false> {};
constexpr auto props_may_omit_ = props_def<true> {};

} // end namespace prop

} // end namespace boost::mqtt5::encoders

#endif // !BOOST_MQTT5_BASE_ENCODERS_HPP
