#ifndef BOOST_HASH2_HAS_CONSTANT_SIZE_HPP_INCLUDED
#define BOOST_HASH2_HAS_CONSTANT_SIZE_HPP_INCLUDED

// Copyright 2024 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <type_traits>
#include <utility>

namespace boost
{

// forward declaration
template<class T, std::size_t N> class array;
    
namespace hash2
{

// forward declaration
template<std::size_t N> class digest;

// detail::has_tuple_size

namespace detail
{

template<class T, class En = void> struct has_tuple_size: std::false_type
{
};

template<class T> struct has_tuple_size<T,
    typename std::enable_if<
        std::tuple_size<T>::value == std::tuple_size<T>::value
    >::type
>: std::true_type
{
};

} // namespace detail

// has_constant_size

template<class T> struct has_constant_size: detail::has_tuple_size<T>
{
};

template<class T, std::size_t N> struct has_constant_size< boost::array<T, N> >: std::true_type
{
};

template<std::size_t N> struct has_constant_size< digest<N> >: std::true_type
{
};

template<class T> struct has_constant_size<T const>: has_constant_size<T>
{
};

} // namespace hash2
} // namespace boost

#endif // #ifndef BOOST_HASH2_HAS_CONSTANT_SIZE_HPP_INCLUDED
