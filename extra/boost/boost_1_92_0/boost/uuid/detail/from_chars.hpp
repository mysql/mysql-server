#ifndef BOOST_UUID_DETAIL_FROM_CHARS_HPP_INCLUDED
#define BOOST_UUID_DETAIL_FROM_CHARS_HPP_INCLUDED

// Copyright 2025 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/uuid/detail/from_chars_generic.hpp>
#include <boost/uuid/detail/from_chars_result.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/detail/config.hpp>
#include <boost/uuid/detail/is_constant_evaluated.hpp>

#if defined(BOOST_UUID_USE_SSE2)
# include <boost/uuid/detail/from_chars_x86.hpp>

#elif defined(BOOST_UUID_REPORT_IMPLEMENTATION)
# include <boost/config/pragma_message.hpp>
  BOOST_PRAGMA_MESSAGE( "Using from_chars_generic.hpp" )

#endif

namespace boost {
namespace uuids {

template<class Ch>
BOOST_UUID_CXX14_CONSTEXPR_RT inline
from_chars_result<Ch> from_chars( Ch const* first, Ch const* last, uuid& u ) noexcept
{
#if defined(BOOST_UUID_USE_SSE2)
    if( detail::is_constant_evaluated_rt() )
    {
        return detail::from_chars_generic( first, last, u );
    }
    else
    {
        return detail::from_chars_simd( first, last, u );
    }
#else
    return detail::from_chars_generic( first, last, u );
#endif
}

}} //namespace boost::uuids

#endif // BOOST_UUID_DETAIL_TO_CHARS_HPP_INCLUDED
