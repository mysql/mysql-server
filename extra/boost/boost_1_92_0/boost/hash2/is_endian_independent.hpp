#ifndef BOOST_HASH2_IS_ENDIAN_INDEPENDENT_HPP_INCLUDED
#define BOOST_HASH2_IS_ENDIAN_INDEPENDENT_HPP_INCLUDED

// Copyright 2017, 2023, 2024 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <type_traits>

namespace boost
{
namespace hash2
{

template<class T> struct is_endian_independent:
    std::integral_constant< bool, sizeof(T) == 1 >
{
};

template<class T> struct is_endian_independent<T const>:
    is_endian_independent<T>
{
};

} // namespace hash2
} // namespace boost

#endif // #ifndef BOOST_HASH2_IS_ENDIAN_INDEPENDENT_HPP_INCLUDED
