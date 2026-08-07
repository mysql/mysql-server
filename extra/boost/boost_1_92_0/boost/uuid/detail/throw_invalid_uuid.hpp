#ifndef BOOST_UUID_DETAIL_THROW_INVALID_UUID_INCLUDED
#define BOOST_UUID_DETAIL_THROW_INVALID_UUID_INCLUDED

// Copyright 2025, 2026 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/uuid/invalid_uuid.hpp>
#include <boost/throw_exception.hpp>
#include <boost/assert/source_location.hpp>

namespace boost {
namespace uuids {
namespace detail {

BOOST_NORETURN inline void throw_invalid_uuid( std::ptrdiff_t pos, from_chars_error err, boost::source_location const& loc = BOOST_CURRENT_LOCATION )
{
    boost::throw_exception( invalid_uuid( pos, err ), loc );
}

}}} // namespace boost::uuids::detail

#endif // #ifndef BOOST_UUID_DETAIL_THROW_INVALID_UUID_INCLUDED
