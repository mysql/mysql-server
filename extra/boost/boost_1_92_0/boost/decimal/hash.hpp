// Copyright 2023 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_DECIMAL_HASH_HPP
#define BOOST_DECIMAL_HASH_HPP

#include <boost/decimal/decimal32_t.hpp>
#include <boost/decimal/decimal64_t.hpp>
#include <boost/decimal/decimal128_t.hpp>
#include <boost/decimal/decimal_fast32_t.hpp>
#include <boost/decimal/decimal_fast64_t.hpp>
#include <boost/decimal/decimal_fast128_t.hpp>
#include <boost/decimal/detail/config.hpp>
#include <boost/decimal/detail/cmath/normalize.hpp>

#ifndef BOOST_DECIMAL_BUILD_MODULE
#include <functional>
#include <cstring>
#endif

namespace std {

template <>
struct hash<boost::decimal::decimal32_t>
{
    // Since the underlying type is a std::uint32_t, we will rely on its hash function from the STL
    auto operator()(const boost::decimal::decimal32_t& v) const noexcept -> std::size_t
    {
        const auto normalized_v {boost::decimal::normalize(v)};

        std::uint32_t bits;
        std::memcpy(&bits, &normalized_v, sizeof(std::uint32_t));

        return std::hash<std::uint32_t>{}(bits);
    }
};

template <>
struct hash<boost::decimal::decimal64_t>
{
    // Since the underlying type is a std::uint64_t, we will rely on its hash function from the STL
    auto operator()(const boost::decimal::decimal64_t& v) const noexcept -> std::size_t
    {
        const auto normalized_v {boost::decimal::normalize(v)};

        std::uint64_t bits;
        std::memcpy(&bits, &normalized_v, sizeof(std::uint64_t));

        return std::hash<std::uint64_t>{}(bits);
    }
};

#if defined(__GNUC__) && __GNUC__ >= 8
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wclass-memaccess"
#endif

template <>
struct hash<boost::decimal::decimal128_t>
{
    // Take the xor of the two words and hash that
    auto operator()(const boost::decimal::decimal128_t& v) const noexcept -> std::size_t
    {
        const auto normalized_v {boost::decimal::normalize(v)};

        boost::int128::uint128_t bits;
        std::memcpy(&bits, &normalized_v, sizeof(boost::int128::uint128_t));

        return std::hash<std::uint64_t>{}(bits.high ^ bits.low);
    }
};

#if defined(__GNUC__) && __GNUC__ >= 8
#  pragma GCC diagnostic pop
#endif

template <>
struct hash<boost::decimal::decimal_fast32_t>
{
    // Need to convert into decimal32_t then apply our memcpy
    auto operator()(const boost::decimal::decimal_fast32_t& v) const noexcept -> std::size_t
    {
        boost::decimal::decimal32_t v_32 {v};
        std::uint32_t bits;
        std::memcpy(&bits, &v_32, sizeof(std::uint32_t));

        return std::hash<std::uint32_t>{}(bits);
    }
};

template <>
struct hash<boost::decimal::decimal_fast64_t>
{
    // Since the underlying type is a std::uint64_t, we will rely on its hash function from the STL
    // First we convert to a decimal64_t, so they will have the same hash value
    auto operator()(const boost::decimal::decimal_fast64_t& v) const noexcept -> std::size_t
    {
        boost::decimal::decimal64_t v_64 {v};
        std::uint64_t bits;
        std::memcpy(&bits, &v_64, sizeof(std::uint64_t));

        return std::hash<std::uint64_t>{}(bits);
    }
};

#if defined(__GNUC__) && __GNUC__ >= 8
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wclass-memaccess"
#endif

template <>
struct hash<boost::decimal::decimal_fast128_t>
{
    // Take the xor of the two words and hash that
    auto operator()(const boost::decimal::decimal_fast128_t& v) const noexcept -> std::size_t
    {
        boost::decimal::decimal128_t v_128 {v};
        boost::int128::uint128_t bits;
        std::memcpy(&bits, &v_128, sizeof(boost::int128::uint128_t));

        return std::hash<std::uint64_t>{}(bits.high ^ bits.low);
    }
};

#if defined(__GNUC__) && __GNUC__ >= 8
#  pragma GCC diagnostic pop
#endif

} // namespace std

#endif //BOOST_DECIMAL_HASH_HPP
