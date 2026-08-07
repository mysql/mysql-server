#ifndef BOOST_UUID_DETAIL_UUID_FROM_STRING_INCLUDED
#define BOOST_UUID_DETAIL_UUID_FROM_STRING_INCLUDED

// Copyright 2026 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/detail/from_chars.hpp>
#include <boost/uuid/detail/throw_invalid_uuid.hpp>
#include <boost/uuid/detail/cstring.hpp>
#include <boost/config.hpp>

namespace boost {
namespace uuids {

namespace detail {

template<class Ch>
BOOST_CXX14_CONSTEXPR
uuid uuid_from_string( Ch const* first, Ch const* last )
{
    uuid u;

    auto r = from_chars( first, last, u );

    if( r.ec != from_chars_error::none )
    {
        detail::throw_invalid_uuid( r.ptr - first, r.ec );
    }

    if( r.ptr != last )
    {
        detail::throw_invalid_uuid( r.ptr - first, from_chars_error::unexpected_extra_input );
    }

    return u;
}

} // namespace detail

template<class Ch>
BOOST_CXX14_CONSTEXPR
uuid uuid_from_string( Ch const* str )
{
    Ch const* first = str;
    Ch const* last = str + detail::strlen_cx( str );

    return detail::uuid_from_string( first, last );
}

template<class Str, class Ch = typename Str::value_type, class Tr = typename Str::traits_type>
BOOST_CXX14_CONSTEXPR
uuid uuid_from_string( Str const& str )
{
    Ch const* first = str.data();
    Ch const* last = str.data() + str.size();

    return detail::uuid_from_string( first, last );
}

}} // namespace boost::uuids

#endif // #ifndef BOOST_UUID_DETAIL_UUID_FROM_STRING_INCLUDED
