#ifndef BOOST_HASH2_IS_CONTIGUOUSLY_HASHABLE_HPP_INCLUDED
#define BOOST_HASH2_IS_CONTIGUOUSLY_HASHABLE_HPP_INCLUDED

// Copyright 2017, 2023, 2024 Peter Dimov.
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/hash2/is_trivially_equality_comparable.hpp>
#include <boost/hash2/is_endian_independent.hpp>
#include <boost/hash2/endian.hpp>
#include <type_traits>
#include <cstddef>

namespace boost
{
namespace hash2
{

template<class T, endian E> struct is_contiguously_hashable:
    std::integral_constant<bool, is_trivially_equality_comparable<T>::value && (E == endian::native || is_endian_independent<T>::value)>
{
};

template<class T, std::size_t N, endian E> struct is_contiguously_hashable<T[N], E>:
    is_contiguously_hashable<T, E>
{
};

} // namespace hash2
} // namespace boost

#endif // #ifndef BOOST_HASH2_IS_CONTIGUOUSLY_HASHABLE_HPP_INCLUDED
