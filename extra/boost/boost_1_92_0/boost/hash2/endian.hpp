// Copyright 2017, 2018 Peter Dimov.
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_HASH2_ENDIAN_HPP_INCLUDED
#define BOOST_HASH2_ENDIAN_HPP_INCLUDED

namespace boost
{
namespace hash2
{

#if defined(_MSC_VER)

enum class endian
{
    little,
    big,
    native = little
};

#else

// GCC 4.6+, Clang 3.2+

enum class endian
{
    little = __ORDER_LITTLE_ENDIAN__,
    big = __ORDER_BIG_ENDIAN__,
    native = __BYTE_ORDER__
};

#endif

} // namespace hash2
} // namespace boost

#endif // #ifndef BOOST_HASH2_ENDIAN_HPP_INCLUDED
