// Copyright Peter Dimov, 2025.
// Copyright Romain Geissler, 2025.
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_LEXICAL_CAST_DETAIL_TYPE_TRAITS_HPP
#define BOOST_LEXICAL_CAST_DETAIL_TYPE_TRAITS_HPP

#include <boost/config.hpp>
#ifdef BOOST_HAS_PRAGMA_ONCE
#   pragma once
#endif

#include <type_traits>

namespace boost { namespace detail { namespace lcast {

// libstdc++ from gcc <= 15 doesn't provide support for __int128 in the standard traits,
// so define them explicitly.
// This was fixed with gcc >= 16, so we may eventually remove this workaround and use
// directly the standard type_traits.

template<class T> struct is_integral: public std::is_integral<T>
{
};

template<class T> struct is_signed: public std::is_signed<T>
{
};

template<class T> struct is_unsigned: public std::is_unsigned<T>
{
};

template<class T> struct make_unsigned: public std::make_unsigned<T>
{
};

#if defined(__SIZEOF_INT128__)

template<> struct is_integral<__int128_t>: public std::true_type
{
};

template<> struct is_integral<__uint128_t>: public std::true_type
{
};

template<> struct is_signed<__int128_t>: public std::true_type
{
};

template<> struct is_signed<__uint128_t>: public std::false_type
{
};

template<> struct is_unsigned<__int128_t>: public std::false_type
{
};

template<> struct is_unsigned<__uint128_t>: public std::true_type
{
};

template<> struct make_unsigned<__int128_t>
{
    typedef __uint128_t type;
};

template<> struct make_unsigned<__uint128_t>
{
    typedef __uint128_t type;
};

#endif

}}}  // namespace boost::detail::lcast

#endif // BOOST_LEXICAL_CAST_DETAIL_TYPE_TRAITS_HPP
