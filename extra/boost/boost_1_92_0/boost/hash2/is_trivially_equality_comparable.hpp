#ifndef BOOST_HASH2_IS_TRIVIALLY_EQUALITY_COMPARABLE_HPP_INCLUDED
#define BOOST_HASH2_IS_TRIVIALLY_EQUALITY_COMPARABLE_HPP_INCLUDED

// Copyright 2017, 2023, 2024 Peter Dimov.
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <type_traits>

namespace boost
{
namespace hash2
{

template<class T> struct is_trivially_equality_comparable:
    std::integral_constant< bool, std::is_integral<T>::value || std::is_enum<T>::value || std::is_pointer<T>::value >
{
};

template<class T> struct is_trivially_equality_comparable<T const>:
    is_trivially_equality_comparable<T>
{
};

} // namespace hash2
} // namespace boost

#endif // #ifndef BOOST_HASH2_IS_TRIVIALLY_EQUALITY_COMPARABLE_HPP_INCLUDED
