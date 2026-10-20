// Copyright 2025 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_DECIMAL_DETAIL_INT128_CONVERSIONS_HPP
#define BOOST_DECIMAL_DETAIL_INT128_CONVERSIONS_HPP

#include <boost/decimal/detail/int128/detail/int128_imp.hpp>
#include <boost/decimal/detail/int128/detail/uint128_imp.hpp>

namespace boost {
namespace int128 {

namespace detail {

template <typename T>
struct valid_overload
{
    static constexpr bool value = std::is_same<T, uint128_t>::value || std::is_same<T, int128_t>::value;
};

template <typename T>
BOOST_DECIMAL_DETAIL_INT128_INLINE_CONSTEXPR bool is_valid_overload_v = valid_overload<T>::value;

} // namespace detail

#if BOOST_DECIMAL_DETAIL_INT128_ENDIAN_LITTLE_BYTE

BOOST_DECIMAL_DETAIL_INT128_HOST_DEVICE constexpr int128_t::int128_t(const uint128_t& v) noexcept : low {v.low}, high {static_cast<std::int64_t>(v.high)} {}

BOOST_DECIMAL_DETAIL_INT128_HOST_DEVICE constexpr uint128_t::uint128_t(const int128_t& v) noexcept : low {v.low}, high {static_cast<std::uint64_t>(v.high)} {}

#else

BOOST_DECIMAL_DETAIL_INT128_HOST_DEVICE constexpr int128_t::int128_t(const uint128_t& v) noexcept : high {static_cast<std::int64_t>(v.high)}, low {v.low} {}

BOOST_DECIMAL_DETAIL_INT128_HOST_DEVICE constexpr uint128_t::uint128_t(const int128_t& v) noexcept : high {static_cast<std::uint64_t>(v.high)}, low {v.low} {}

#endif // BOOST_DECIMAL_DETAIL_INT128_ENDIAN_LITTLE_BYTE

//=====================================
// Conversion Operators
//=====================================

BOOST_DECIMAL_DETAIL_INT128_HOST_DEVICE constexpr int128_t::operator uint128_t() const noexcept
{
    return uint128_t{static_cast<std::uint64_t>(this->high), static_cast<std::uint64_t>(this->low)};
}

BOOST_DECIMAL_DETAIL_INT128_HOST_DEVICE constexpr uint128_t::operator int128_t() const noexcept
{
    return int128_t{static_cast<std::int64_t>(this->high), static_cast<std::uint64_t>(this->low)};
}

//=====================================
// Comparison Operators
//=====================================

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4127)
#endif

template <typename T, typename U, std::enable_if_t<detail::is_valid_overload_v<T> && detail::is_valid_overload_v<U> && !std::is_same<T, U>::value, bool> = true>
BOOST_DECIMAL_DETAIL_INT128_HOST_DEVICE constexpr bool operator==(const T lhs, const U rhs) noexcept
{
    #ifndef BOOST_DECIMAL_DETAIL_INT128_ALLOW_SIGN_COMPARE

    static_assert(std::is_same<T, U>::value, "Sign Compare Error, cast one type to the other for this operation");
    static_cast<void>(lhs);
    static_cast<void>(rhs);
    return true;

    #else

    BOOST_DECIMAL_DETAIL_INT128_IF_CONSTEXPR (std::is_same<T, int128_t>::value)
    {
        if (lhs < T{0})
        {
            return false;
        }

        return static_cast<uint128_t>(lhs) == rhs;
    }
    else
    {
        if (rhs < T{0})
        {
            return false;
        }

        return lhs == static_cast<uint128_t>(rhs);
    }

    #endif
}

template <typename T, typename U, std::enable_if_t<detail::is_valid_overload_v<T> && detail::is_valid_overload_v<U> && !std::is_same<T, U>::value, bool> = true>
BOOST_DECIMAL_DETAIL_INT128_HOST_DEVICE constexpr bool operator!=(const T lhs, const U rhs) noexcept
{
    #ifndef BOOST_DECIMAL_DETAIL_INT128_ALLOW_SIGN_COMPARE

    static_assert(std::is_same<T, U>::value, "Sign Compare Error, cast one type to the other for this operation");
    static_cast<void>(lhs);
    static_cast<void>(rhs);
    return true;

    #else

    BOOST_DECIMAL_DETAIL_INT128_IF_CONSTEXPR (std::is_same<T, int128_t>::value)
    {
        if (lhs < T{0})
        {
            return true;
        }

        return static_cast<uint128_t>(lhs) != rhs;
    }
    else
    {
        if (rhs < T{0})
        {
            return true;
        }

        return lhs != static_cast<uint128_t>(rhs);
    }

    #endif
}

template <typename T, typename U, std::enable_if_t<detail::is_valid_overload_v<T> && detail::is_valid_overload_v<U> && !std::is_same<T, U>::value, bool> = true>
BOOST_DECIMAL_DETAIL_INT128_HOST_DEVICE constexpr bool operator<(const T lhs, const U rhs) noexcept
{
    #ifndef BOOST_DECIMAL_DETAIL_INT128_ALLOW_SIGN_COMPARE

    static_assert(std::is_same<T, U>::value, "Sign Compare Error, cast one type to the other for this operation");
    static_cast<void>(lhs);
    static_cast<void>(rhs);
    return true;

    #else

    BOOST_DECIMAL_DETAIL_INT128_IF_CONSTEXPR (std::is_same<T, int128_t>::value)
    {
        if (lhs < T{0})
        {
            return true;
        }

        return static_cast<uint128_t>(lhs) < rhs;
    }
    else
    {
        if (rhs < T{0})
        {
            return false;
        }

        return lhs < static_cast<uint128_t>(rhs);
    }

    #endif
}

template <typename T, typename U, std::enable_if_t<detail::is_valid_overload_v<T> && detail::is_valid_overload_v<U> && !std::is_same<T, U>::value, bool> = true>
BOOST_DECIMAL_DETAIL_INT128_HOST_DEVICE constexpr bool operator<=(const T lhs, const U rhs) noexcept
{
    #ifndef BOOST_DECIMAL_DETAIL_INT128_ALLOW_SIGN_COMPARE

    static_assert(std::is_same<T, U>::value, "Sign Compare Error, cast one type to the other for this operation");
    static_cast<void>(lhs);
    static_cast<void>(rhs);
    return true;

    #else

    BOOST_DECIMAL_DETAIL_INT128_IF_CONSTEXPR (std::is_same<T, int128_t>::value)
    {
        if (lhs < T{0})
        {
            return true;
        }

        return static_cast<uint128_t>(lhs) <= rhs;
    }
    else
    {
        if (rhs < T{0})
        {
            return false;
        }

        return lhs <= static_cast<uint128_t>(rhs);
    }

    #endif
}

template <typename T, typename U, std::enable_if_t<detail::is_valid_overload_v<T> && detail::is_valid_overload_v<U> && !std::is_same<T, U>::value, bool> = true>
BOOST_DECIMAL_DETAIL_INT128_HOST_DEVICE constexpr bool operator>(const T lhs, const U rhs) noexcept
{
    #ifndef BOOST_DECIMAL_DETAIL_INT128_ALLOW_SIGN_COMPARE

    static_assert(std::is_same<T, U>::value, "Sign Compare Error, cast one type to the other for this operation");
    static_cast<void>(lhs);
    static_cast<void>(rhs);
    return true;

    #else

    BOOST_DECIMAL_DETAIL_INT128_IF_CONSTEXPR (std::is_same<T, int128_t>::value)
    {
        if (lhs < T{0})
        {
            return false;
        }

        return static_cast<uint128_t>(lhs) > rhs;
    }
    else
    {
        if (rhs < T{0})
        {
            return true;
        }

        return lhs > static_cast<uint128_t>(rhs);
    }

    #endif
}

template <typename T, typename U, std::enable_if_t<detail::is_valid_overload_v<T> && detail::is_valid_overload_v<U> && !std::is_same<T, U>::value, bool> = true>
BOOST_DECIMAL_DETAIL_INT128_HOST_DEVICE constexpr bool operator>=(const T lhs, const U rhs) noexcept
{
    #ifndef BOOST_DECIMAL_DETAIL_INT128_ALLOW_SIGN_COMPARE

    static_assert(std::is_same<T, U>::value, "Sign Compare Error, cast one type to the other for this operation");
    static_cast<void>(lhs);
    static_cast<void>(rhs);
    return true;

    #else

    BOOST_DECIMAL_DETAIL_INT128_IF_CONSTEXPR (std::is_same<T, int128_t>::value)
    {
        if (lhs < T{0})
        {
            return false;
        }

        return static_cast<uint128_t>(lhs) >= rhs;
    }
    else
    {
        if (rhs < T{0})
        {
            return true;
        }

        return lhs >= static_cast<uint128_t>(rhs);
    }

    #endif
}

//=====================================
// Arithmetic Operators
//=====================================

template <typename T, typename U, std::enable_if_t<detail::is_valid_overload_v<T> && detail::is_valid_overload_v<U> && !std::is_same<T, U>::value, bool> = true>
BOOST_DECIMAL_DETAIL_INT128_HOST_DEVICE constexpr uint128_t operator+(const T lhs, const U rhs) noexcept
{
    #ifndef BOOST_DECIMAL_DETAIL_INT128_ALLOW_SIGN_CONVERSION

    static_assert(std::is_same<T, U>::value, "Sign Conversion Error, cast one type to the other for this operation");
    static_cast<void>(rhs);
    return static_cast<uint128_t>(lhs);

    #else

    return static_cast<uint128_t>(lhs) + static_cast<uint128_t>(rhs);

    #endif
}

template <typename T, typename U, std::enable_if_t<detail::is_valid_overload_v<T> && detail::is_valid_overload_v<U> && !std::is_same<T, U>::value, bool> = true>
BOOST_DECIMAL_DETAIL_INT128_HOST_DEVICE constexpr uint128_t operator-(const T lhs, const U rhs) noexcept
{
    #ifndef BOOST_DECIMAL_DETAIL_INT128_ALLOW_SIGN_CONVERSION

    static_assert(std::is_same<T, U>::value, "Sign Conversion Error, cast one type to the other for this operation");
    static_cast<void>(rhs);
    return static_cast<uint128_t>(lhs);

    #else

    return static_cast<uint128_t>(lhs) - static_cast<uint128_t>(rhs);

    #endif
}

template <typename T, typename U, std::enable_if_t<detail::is_valid_overload_v<T> && detail::is_valid_overload_v<U> && !std::is_same<T, U>::value, bool> = true>
BOOST_DECIMAL_DETAIL_INT128_HOST_DEVICE constexpr uint128_t operator*(const T lhs, const U rhs) noexcept
{
    #ifndef BOOST_DECIMAL_DETAIL_INT128_ALLOW_SIGN_CONVERSION

    static_assert(std::is_same<T, U>::value, "Sign Conversion Error, cast one type to the other for this operation");
    static_cast<void>(rhs);
    return static_cast<uint128_t>(lhs);

    #else

    return static_cast<uint128_t>(lhs) * static_cast<uint128_t>(rhs);

    #endif
}

template <typename T, typename U, std::enable_if_t<detail::is_valid_overload_v<T> && detail::is_valid_overload_v<U> && !std::is_same<T, U>::value, bool> = true>
BOOST_DECIMAL_DETAIL_INT128_HOST_DEVICE constexpr uint128_t operator/(const T lhs, const U rhs) noexcept
{
    #ifndef BOOST_DECIMAL_DETAIL_INT128_ALLOW_SIGN_CONVERSION

    static_assert(std::is_same<T, U>::value, "Sign Conversion Error, cast one type to the other for this operation");
    static_cast<void>(rhs);
    return static_cast<uint128_t>(lhs);

    #else

    return static_cast<uint128_t>(lhs) / static_cast<uint128_t>(rhs);

    #endif
}

template <typename T, typename U, std::enable_if_t<detail::is_valid_overload_v<T> && detail::is_valid_overload_v<U> && !std::is_same<T, U>::value, bool> = true>
BOOST_DECIMAL_DETAIL_INT128_HOST_DEVICE constexpr uint128_t operator%(const T lhs, const U rhs) noexcept
{
    #ifndef BOOST_DECIMAL_DETAIL_INT128_ALLOW_SIGN_CONVERSION

    static_assert(std::is_same<T, U>::value, "Sign Conversion Error, cast one type to the other for this operation");
    static_cast<void>(rhs);
    return static_cast<uint128_t>(lhs);

    #else

    return static_cast<uint128_t>(lhs) % static_cast<uint128_t>(rhs);

    #endif
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif

} // namespace int128
} // namespace boost

#endif // BOOST_DECIMAL_DETAIL_INT128_CONVERSIONS_HPP
