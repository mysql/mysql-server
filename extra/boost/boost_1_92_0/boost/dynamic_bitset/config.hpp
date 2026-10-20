// -----------------------------------------------------------
//
//   Copyright (c) 2001-2002 Chuck Allison and Jeremy Siek
//      Copyright (c) 2003-2006, 2008, 2025 Gennaro Prota
//
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
//
// -----------------------------------------------------------

#ifndef BOOST_DYNAMIC_BITSET_CONFIG_HPP_GP_20040424
#define BOOST_DYNAMIC_BITSET_CONFIG_HPP_GP_20040424

#include "boost/config.hpp"

// no-op function to workaround gcc bug c++/8419
//
namespace boost {
namespace detail {
template< typename T >
T
make_non_const( T t )
{
    return t;
}
}
}

#if defined( __GNUC__ )
#    define BOOST_DYNAMIC_BITSET_WRAP_CONSTANT( expr ) \
        ( boost::detail::make_non_const( expr ) )
#else
#    define BOOST_DYNAMIC_BITSET_WRAP_CONSTANT( expr ) ( expr )
#endif

#if ! defined( BOOST_NO_CXX11_HDR_FUNCTIONAL ) && ! defined( BOOST_DYNAMIC_BITSET_NO_STD_HASH )
#    define BOOST_DYNAMIC_BITSET_SPECIALIZE_STD_HASH
#endif

#if ( defined( _MSVC_LANG ) && _MSVC_LANG >= 201703L ) || __cplusplus >= 201703L
#    define BOOST_DYNAMIC_BITSET_USE_CPP17_OR_LATER
#endif

#if ( defined( _MSVC_LANG ) && _MSVC_LANG >= 202002L ) || __cplusplus >= 202002L
#    define BOOST_DYNAMIC_BITSET_CONSTEXPR20 constexpr
#else
#    define BOOST_DYNAMIC_BITSET_CONSTEXPR20
#endif

#endif // include guard
