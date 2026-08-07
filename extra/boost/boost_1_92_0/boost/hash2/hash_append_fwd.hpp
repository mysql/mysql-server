#ifndef BOOST_HASH2_HASH_APPEND_FWD_HPP_INCLUDED
#define BOOST_HASH2_HASH_APPEND_FWD_HPP_INCLUDED

// Copyright 2024 Peter Dimov.
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/config.hpp>

namespace boost
{
namespace hash2
{

#if defined(BOOST_GCC) && BOOST_GCC < 120000

// Due to a bug in GCC 11 and earlier, the default argument
// for Flavor needs to be present on the first declaration

struct default_flavor;

template<class Hash, class Flavor = default_flavor, class T> BOOST_CXX14_CONSTEXPR void hash_append( Hash& h, Flavor const& f, T const& v );
template<class Hash, class Flavor = default_flavor, class It> BOOST_CXX14_CONSTEXPR void hash_append_range( Hash& h, Flavor const& f, It first, It last );
template<class Hash, class Flavor = default_flavor, class T> BOOST_CXX14_CONSTEXPR void hash_append_size( Hash& h, Flavor const& f, T const& v );
template<class Hash, class Flavor = default_flavor, class It> BOOST_CXX14_CONSTEXPR void hash_append_range_and_size( Hash& h, Flavor const& f, It first, It last );
template<class Hash, class Flavor = default_flavor, class It> BOOST_CXX14_CONSTEXPR void hash_append_unordered_range( Hash& h, Flavor const& f, It first, It last );

#else

template<class Hash, class Flavor, class T> BOOST_CXX14_CONSTEXPR void hash_append( Hash& h, Flavor const& f, T const& v );
template<class Hash, class Flavor, class It> BOOST_CXX14_CONSTEXPR void hash_append_range( Hash& h, Flavor const& f, It first, It last );
template<class Hash, class Flavor, class T> BOOST_CXX14_CONSTEXPR void hash_append_size( Hash& h, Flavor const& f, T const& v );
template<class Hash, class Flavor, class It> BOOST_CXX14_CONSTEXPR void hash_append_range_and_size( Hash& h, Flavor const& f, It first, It last );
template<class Hash, class Flavor, class It> BOOST_CXX14_CONSTEXPR void hash_append_unordered_range( Hash& h, Flavor const& f, It first, It last );

#endif

struct hash_append_tag;

} // namespace hash2
} // namespace boost

#endif // #ifndef BOOST_HASH2_HASH_APPEND_FWD_HPP_INCLUDED
