// -----------------------------------------------------------
//
//   Copyright (c) 2001-2002 Chuck Allison and Jeremy Siek
//   Copyright (c) 2003-2006, 2008, 2025 Gennaro Prota
//   Copyright (c) 2014 Glen Joseph Fernandes
//       (glenjofe@gmail.com)
//   Copyright (c) 2018 Evgeny Shulgin
//   Copyright (c) 2019 Andrey Semashev
//
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
//
// -----------------------------------------------------------

#ifndef BOOST_DETAIL_DYNAMIC_BITSET_HPP
#define BOOST_DETAIL_DYNAMIC_BITSET_HPP

#include <cstddef>
#include <type_traits>
#include <utility>

namespace boost {

namespace detail {
namespace dynamic_bitset_impl {

template< typename AllocatorOrContainer, typename Block >
class is_container
{
private:
    template< typename U >
    static decltype( std::declval< U >().resize( std::size_t{} ), std::declval< U >()[ 0 ], typename U::value_type(), std::is_same< typename U::value_type, Block >{}, std::true_type{} ) test( int );

    template< typename >
    static std::false_type test( ... );

public:
    static constexpr bool value = decltype( test< AllocatorOrContainer >( 0 ) )::value;
};

template< typename AllocatorOrContainer, bool IsContainer >
class allocator_type_extractor_impl;

template< typename AllocatorOrContainer >
class allocator_type_extractor_impl< AllocatorOrContainer, false >
{
public:
    typedef AllocatorOrContainer type;
};

template< typename AllocatorOrContainer >
class allocator_type_extractor_impl< AllocatorOrContainer, true >
{
public:
    typedef typename AllocatorOrContainer::allocator_type type;
};

template< typename AllocatorOrContainer, typename Block >
class allocator_type_extractor
{
public:
    typedef typename allocator_type_extractor_impl<
        AllocatorOrContainer,
        is_container< AllocatorOrContainer, Block >::value >::type type;
};

template< typename T, int amount, int width /* = default */ >
struct shifter
{
    static BOOST_DYNAMIC_BITSET_CONSTEXPR20 void
    right_shift( T & v )
    {
        amount >= width ? ( v = 0 )
                        : ( v >>= BOOST_DYNAMIC_BITSET_WRAP_CONSTANT( amount ) );
    }
};

template< bool value >
struct value_to_type
{
};

// for static_asserts
template< typename T >
struct allowed_block_type
{
    enum
    {
        value = T( -1 ) > 0 // ensure T has no sign
    };
};

template<>
struct allowed_block_type< bool >
{
    enum
    {
        value = false
    };
};

template< typename T >
using is_numeric = std::is_integral< T >; // floating points intentionally excluded

} // dynamic_bitset_impl
} // namespace detail

} // namespace boost

#endif // include guard
