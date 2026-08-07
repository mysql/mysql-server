#ifndef BOOST_UUID_DETAIL_UUID_GENERIC_IPP_INCLUDED
#define BOOST_UUID_DETAIL_UUID_GENERIC_IPP_INCLUDED

// Copyright 2024 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/uuid/detail/endian.hpp>
#include <boost/uuid/detail/cstring.hpp>
#include <boost/uuid/detail/is_constant_evaluated.hpp>
#include <boost/config.hpp>
#include <cstdint>

#if defined(BOOST_UUID_REPORT_IMPLEMENTATION)

#include <boost/config/pragma_message.hpp>
BOOST_PRAGMA_MESSAGE( "Using uuid_generic.ipp" )

#endif

namespace boost {
namespace uuids {

BOOST_UUID_CXX14_CONSTEXPR_RT inline bool operator==( uuid const& lhs, uuid const& rhs ) noexcept
{
    if( detail::is_constant_evaluated_rt() )
    {
        return detail::memcmp_rt( lhs.data(), rhs.data(), 16 ) == 0;
    }
    else
    {
        std::uint64_t v1 = detail::load_native_u64( lhs.data() + 0 );
        std::uint64_t w1 = detail::load_native_u64( lhs.data() + 8 );

        std::uint64_t v2 = detail::load_native_u64( rhs.data() + 0 );
        std::uint64_t w2 = detail::load_native_u64( rhs.data() + 8 );

        return v1 == v2 && w1 == w2;
    }
}

BOOST_UUID_CXX14_CONSTEXPR_RT inline bool operator<( uuid const& lhs, uuid const& rhs ) noexcept
{
    if( detail::is_constant_evaluated_rt() )
    {
        return detail::memcmp_rt( lhs.data(), rhs.data(), 16 ) < 0;
    }
    else
    {
        std::uint64_t v1 = detail::load_big_u64( lhs.data() + 0 );
        std::uint64_t w1 = detail::load_big_u64( lhs.data() + 8 );

        std::uint64_t v2 = detail::load_big_u64( rhs.data() + 0 );
        std::uint64_t w2 = detail::load_big_u64( rhs.data() + 8 );

        return v1 < v2 || ( !( v2 < v1 ) && w1 < w2 );
    }
}

#if defined(BOOST_UUID_HAS_THREE_WAY_COMPARISON)

BOOST_UUID_CXX14_CONSTEXPR_RT inline std::strong_ordering operator<=> (uuid const& lhs, uuid const& rhs) noexcept
{
    if( detail::is_constant_evaluated_rt() )
    {
        return detail::memcmp_rt( lhs.data(), rhs.data(), 16 ) <=> 0;
    }
    else
    {
        std::uint64_t v1 = detail::load_big_u64( lhs.data() + 0 );
        std::uint64_t w1 = detail::load_big_u64( lhs.data() + 8 );

        std::uint64_t v2 = detail::load_big_u64( rhs.data() + 0 );
        std::uint64_t w2 = detail::load_big_u64( rhs.data() + 8 );

        if( v1 < v2 ) return std::strong_ordering::less;
        if( v1 > v2 ) return std::strong_ordering::greater;

        return w1 <=> w2;
    }
}

#endif

} // namespace uuids
} // namespace boost

#endif // BOOST_UUID_DETAIL_UUID_GENERIC_IPP_INCLUDED
