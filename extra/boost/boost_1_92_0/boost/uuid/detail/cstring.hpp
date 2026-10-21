#ifndef BOOST_UUID_DETAIL_CSTRING_INCLUDED
#define BOOST_UUID_DETAIL_CSTRING_INCLUDED

// Copyright 2025 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/uuid/detail/is_constant_evaluated.hpp>
#include <boost/config.hpp>
#include <string>
#include <cstddef>
#include <cstring>

#if defined(__has_builtin)
#if __has_builtin(__builtin_memcpy)
#define BOOST_UUID_DETAIL_HAS_BUILTIN_MEMCPY
#endif
#if __has_builtin(__builtin_memcmp)
#define BOOST_UUID_DETAIL_HAS_BUILTIN_MEMCMP
#endif
#elif defined(BOOST_GCC)
#define BOOST_UUID_DETAIL_HAS_BUILTIN_MEMCPY
#define BOOST_UUID_DETAIL_HAS_BUILTIN_MEMCMP
#endif

#if defined(BOOST_UUID_DETAIL_HAS_BUILTIN_MEMCPY)
#define BOOST_UUID_DETAIL_MEMCPY __builtin_memcpy
#else
#define BOOST_UUID_DETAIL_MEMCPY std::memcpy
#endif

#if defined(BOOST_UUID_DETAIL_HAS_BUILTIN_MEMCMP)
#define BOOST_UUID_DETAIL_MEMCMP __builtin_memcmp
#else
#define BOOST_UUID_DETAIL_MEMCMP std::memcmp
#endif

namespace boost {
namespace uuids {
namespace detail {

// memcpy

// Note: The function below is a template to prevent an early check whether the function body can ever be evaluated in the context of a constant expression.
//       It can't, and that causes compilation errors with clang. The function must still be marked as constexpr to be able to call it in other
//       functions that are constexpr, even if such calls are never evaluated in a constant expression. Otherwise, gcc 5 through 8 gets upset.

template< typename = void >
BOOST_CXX14_CONSTEXPR BOOST_FORCEINLINE void memcpy( void* dest, void const* src, std::size_t n ) noexcept
{
    BOOST_UUID_DETAIL_MEMCPY( dest, src, n );
}

BOOST_CXX14_CONSTEXPR inline void memcpy_cx( unsigned char* dest, unsigned char const* src, std::size_t n ) noexcept
{
    if( is_constant_evaluated_cx() )
    {
        for( std::size_t i = 0; i < n; ++i ) dest[ i ] = src[ i ];
    }
    else
    {
        BOOST_UUID_DETAIL_MEMCPY( dest, src, n );
    }
}

BOOST_UUID_CXX14_CONSTEXPR_RT inline void memcpy_rt( unsigned char* dest, unsigned char const* src, std::size_t n ) noexcept
{
    if( is_constant_evaluated_rt() )
    {
        for( std::size_t i = 0; i < n; ++i ) dest[ i ] = src[ i ];
    }
    else
    {
        BOOST_UUID_DETAIL_MEMCPY( dest, src, n );
    }
}

// memcmp

BOOST_CXX14_CONSTEXPR inline int memcmp_cx( unsigned char const* s1, unsigned char const* s2, std::size_t n ) noexcept
{
    if( is_constant_evaluated_cx() )
    {
        for( std::size_t i = 0; i < n; ++i )
        {
            if( s1[ i ] < s2[ i ] ) return -1;
            if( s1[ i ] > s2[ i ] ) return +1;
        }

        return 0;
    }
    else
    {
        return BOOST_UUID_DETAIL_MEMCMP( s1, s2, n );
    }
}

BOOST_UUID_CXX14_CONSTEXPR_RT inline int memcmp_rt( unsigned char const* s1, unsigned char const* s2, std::size_t n ) noexcept
{
    if( is_constant_evaluated_rt() )
    {
        for( std::size_t i = 0; i < n; ++i )
        {
            if( s1[ i ] < s2[ i ] ) return -1;
            if( s1[ i ] > s2[ i ] ) return +1;
        }

        return 0;
    }
    else
    {
        return BOOST_UUID_DETAIL_MEMCMP( s1, s2, n );
    }
}

// strlen

template<class Ch>
BOOST_CXX14_CONSTEXPR inline std::size_t strlen_cx( Ch const* s ) noexcept
{
    if( is_constant_evaluated_cx() )
    {
        std::size_t r = 0;

        while( *s ) ++s, ++r;

        return r;
    }
    else
    {
        return std::char_traits<Ch>::length( s );
    }
}

}}} // namespace boost::uuids::detail

#undef BOOST_UUID_DETAIL_MEMCMP
#undef BOOST_UUID_DETAIL_MEMCPY

#endif // #ifndef BOOST_UUID_DETAIL_CSTRING_INCLUDED
