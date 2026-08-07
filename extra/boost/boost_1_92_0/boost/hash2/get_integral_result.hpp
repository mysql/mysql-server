#ifndef BOOST_HASH2_GET_INTEGRAL_RESULT_HPP_INCLUDED
#define BOOST_HASH2_GET_INTEGRAL_RESULT_HPP_INCLUDED

// Copyright 2017, 2018 Peter Dimov.
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/hash2/detail/read.hpp>
#include <type_traits>
#include <limits>
#include <cstddef>

namespace boost
{
namespace hash2
{

namespace detail
{

// contraction

// 2 -> 1, 4 -> x
template<class R>
constexpr typename std::enable_if<sizeof(R) <= 4, std::uint32_t>::type
    get_result_multiplier()
{
    return 0xBF3D6763u;
}

// 8 -> x
template<class R>
constexpr typename std::enable_if<sizeof(R) == 8, std::uint64_t>::type
    get_result_multiplier()
{
    return 0x99EBE72FE70129CBull;
}

} // namespace detail

// contraction

template<class T, class Hash, class R = typename Hash::result_type>
    typename std::enable_if<std::is_integral<R>::value && (sizeof(R) > sizeof(T)), T>::type
    get_integral_result( Hash& h )
{
    static_assert( std::is_integral<T>::value, "T must be integral" );
    static_assert( !std::is_same<typename std::remove_cv<T>::type, bool>::value, "T must not be bool" );

    static_assert( std::is_unsigned<R>::value, "Hash::result_type must be unsigned" );

    typedef typename std::make_unsigned<T>::type U;

    constexpr auto m = detail::get_result_multiplier<R>();

    auto r = h.result();
    return static_cast<T>( static_cast<U>( ( r * m ) >> ( std::numeric_limits<R>::digits - std::numeric_limits<U>::digits ) ) );
}

// identity

template<class T, class Hash, class R = typename Hash::result_type>
    typename std::enable_if<std::is_integral<R>::value && sizeof(R) == sizeof(T), T>::type
    get_integral_result( Hash& h )
{
    static_assert( std::is_integral<T>::value, "T must be integral" );
    static_assert( !std::is_same<typename std::remove_cv<T>::type, bool>::value, "T must not be bool" );

    static_assert( std::is_unsigned<R>::value, "Hash::result_type must be unsigned" );

    typedef typename std::make_unsigned<T>::type U;

    auto r = h.result();
    return static_cast<T>( static_cast<U>( r ) );
}

// expansion

template<class T, class Hash, class R = typename Hash::result_type>
    typename std::enable_if<std::is_integral<R>::value && (sizeof(R) < sizeof(T)), T>::type
    get_integral_result( Hash& h )
{
    static_assert( std::is_integral<T>::value, "T must be integral" );
    static_assert( !std::is_same<typename std::remove_cv<T>::type, bool>::value, "T must not be bool" );

    static_assert( std::is_unsigned<R>::value, "Hash::result_type must be unsigned" );

    typedef typename std::make_unsigned<T>::type U;

    constexpr auto rd = std::numeric_limits<R>::digits;
    constexpr auto ud = std::numeric_limits<U>::digits;

    U u = 0;

    for( int i = 0; i < ud; i += rd )
    {
        auto r = h.result();
        u += static_cast<U>( r ) << i;
    }

    return static_cast<T>( u );
}

// array-like R

template<class T, class Hash, class R = typename Hash::result_type>
    typename std::enable_if< !std::is_integral<R>::value, T >::type
    get_integral_result( Hash& h )
{
    static_assert( std::is_integral<T>::value, "T must be integral" );
    static_assert( !std::is_same<typename std::remove_cv<T>::type, bool>::value, "T must not be bool" );

    static_assert( R().size() >= 8, "Array-like result type is too short" );

    auto r = h.result();
    return static_cast<T>( detail::read64le( r.data() ) );
}

} // namespace hash2
} // namespace boost

#endif // #ifndef BOOST_HASH2_GET_INTEGRAL_RESULT_HPP_INCLUDED
