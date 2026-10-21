#ifndef BOOST_HASH2_DETAIL_BIT_CAST_HPP_INCLUDED
#define BOOST_HASH2_DETAIL_BIT_CAST_HPP_INCLUDED

// Copyright 2024 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/hash2/detail/config.hpp>
#include <boost/config.hpp>
#include <type_traits>
#include <cstring>

namespace boost
{
namespace hash2
{
namespace detail
{

template<class To, class From> BOOST_CXX14_CONSTEXPR To bit_cast( From const& from ) noexcept
{
    static_assert( sizeof(From) == sizeof(To), "Types must be the same size" );

#if defined(BOOST_LIBSTDCXX_VERSION) && BOOST_LIBSTDCXX_VERSION < 50000

    // std::is_trivially_copyable doesn't exist in libstdc++ 4.x

#else

    static_assert( std::is_trivially_copyable<From>::value, "Types must be trivially copyable" );
    static_assert( std::is_trivially_copyable<To>::value, "Types must be trivially copyable" );

#endif

#if defined(BOOST_HASH2_HAS_BUILTIN_BIT_CAST)

    return __builtin_bit_cast( To, from );

#else

    To to{};
    std::memcpy( &to, &from, sizeof(From) );
    return to;

#endif
}

} // namespace detail
} // namespace hash2
} // namespace boost

#endif // #ifndef BOOST_HASH2_DETAIL_BIT_CAST_HPP_INCLUDED
