#ifndef BOOST_HASH2_FLAVOR_HPP_INCLUDED
#define BOOST_HASH2_FLAVOR_HPP_INCLUDED

// Copyright 2024 Peter Dimov.
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/hash2/endian.hpp>
#include <cstdint>

namespace boost
{
namespace hash2
{

struct default_flavor
{
    using size_type = std::uint32_t;
    static constexpr auto byte_order = endian::native;
};

struct little_endian_flavor
{
    using size_type = std::uint32_t;
    static constexpr auto byte_order = endian::little;
};

struct big_endian_flavor
{
    using size_type = std::uint32_t;
    static constexpr auto byte_order = endian::big;
};

} // namespace hash2
} // namespace boost

#endif // #ifndef BOOST_HASH2_FLAVOR_HPP_INCLUDED
