// -----------------------------------------------------------
//
//   Copyright (c) 2001-2002 Chuck Allison and Jeremy Siek
//      Copyright (c) 2003-2006, 2008, 2025 Gennaro Prota
//             Copyright (c) 2014 Ahmed Charles
//
// Copyright (c) 2014 Glen Joseph Fernandes
// (glenjofe@gmail.com)
//
// Copyright (c) 2014 Riccardo Marcangelo
//             Copyright (c) 2018 Evgeny Shulgin
//
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
//
// -----------------------------------------------------------

#include "boost/assert.hpp"
#include "boost/core/bit.hpp"
#include "boost/core/no_exceptions_support.hpp"
#include "boost/dynamic_bitset/detail/lowest_bit.hpp"
#include "boost/functional/hash/hash.hpp"
#include "boost/throw_exception.hpp"
#include <algorithm>
#include <climits>
#include <istream>
#include <locale>
#include <ostream>
#include <stdexcept>
#include <utility>

namespace boost {

template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20
dynamic_bitset< Block, AllocatorOrContainer >::reference::reference( block_type & b, int pos )
    : m_block( b ), m_mask( ( BOOST_ASSERT( pos < bits_per_block ), block_type( 1 ) << pos ) )
{
}

template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 dynamic_bitset< Block, AllocatorOrContainer >::reference::reference( const reference & other ) = default;

template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 dynamic_bitset< Block, AllocatorOrContainer >::reference::
                                 operator bool() const
{
    return ( m_block & m_mask ) != 0;
}

template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 bool
dynamic_bitset< Block, AllocatorOrContainer >::reference::operator~() const
{
    return ( m_block & m_mask ) == 0;
}

template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 typename dynamic_bitset< Block, AllocatorOrContainer >::reference &
dynamic_bitset< Block, AllocatorOrContainer >::reference::flip()
{
    do_flip();
    return *this;
}

template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 typename dynamic_bitset< Block, AllocatorOrContainer >::reference &
dynamic_bitset< Block, AllocatorOrContainer >::reference::operator=( bool x )
{
    do_assign( x );
    return *this;
}

template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 typename dynamic_bitset< Block, AllocatorOrContainer >::reference &
dynamic_bitset< Block, AllocatorOrContainer >::reference::operator=( const reference & rhs )
{
    do_assign( rhs );
    return *this;
}

template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 typename dynamic_bitset< Block, AllocatorOrContainer >::reference &
dynamic_bitset< Block, AllocatorOrContainer >::reference::operator|=( bool x )
{
    if ( x ) {
        do_set();
    }
    return *this;
}

template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 typename dynamic_bitset< Block, AllocatorOrContainer >::reference &
dynamic_bitset< Block, AllocatorOrContainer >::reference::operator&=( bool x )
{
    if ( ! x ) {
        do_reset();
    }
    return *this;
}

template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 typename dynamic_bitset< Block, AllocatorOrContainer >::reference &
dynamic_bitset< Block, AllocatorOrContainer >::reference::operator^=( bool x )
{
    if ( x ) {
        do_flip();
    }
    return *this;
}

template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 typename dynamic_bitset< Block, AllocatorOrContainer >::reference &
dynamic_bitset< Block, AllocatorOrContainer >::reference::operator-=( bool x )
{
    if ( x ) {
        do_reset();
    }
    return *this;
}

template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 void
dynamic_bitset< Block, AllocatorOrContainer >::reference::do_set()
{
    m_block |= m_mask;
}

template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 void
dynamic_bitset< Block, AllocatorOrContainer >::reference::do_reset()
{
    m_block &= ~m_mask;
}

template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 void
dynamic_bitset< Block, AllocatorOrContainer >::reference::do_flip()
{
    m_block ^= m_mask;
}

template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 void
dynamic_bitset< Block, AllocatorOrContainer >::reference::do_assign( bool x )
{
    if ( x ) {
        do_set();
    } else {
        do_reset();
    }
}

template< typename Iterator >
BOOST_DYNAMIC_BITSET_CONSTEXPR20
bit_iterator_base< Iterator >::bit_iterator_base( Iterator block_iterator, int bit_index )
    : m_block_iterator( block_iterator )
    , m_bit_index( bit_index )
{
    BOOST_ASSERT( 0 <= bit_index && bit_index < bits_per_block );
}

template< typename Iterator >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 void
bit_iterator_base< Iterator >::increment()
{
    ++m_bit_index;
    if ( m_bit_index == bits_per_block ) {
        m_bit_index = 0;
        ++m_block_iterator;
    }
}

template< typename Iterator >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 void
bit_iterator_base< Iterator >::decrement()
{
    --m_bit_index;
    if ( m_bit_index < 0 ) {
        m_bit_index = bits_per_block - 1;
        --m_block_iterator;
    }
}

template< typename Iterator >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 void
bit_iterator_base< Iterator >::add( typename std::iterator_traits< Iterator >::difference_type n )
{
    typename Iterator::difference_type d = m_bit_index + n;
    m_block_iterator += d / bits_per_block;
    d %= bits_per_block;
    if ( d < 0 ) {
        d += bits_per_block;
        --m_block_iterator;
    }
    m_bit_index = static_cast< int >( d );
}

template< typename Iterator >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 bool
operator==( const bit_iterator_base< Iterator > & lhs, const bit_iterator_base< Iterator > & rhs )
{
    return lhs.m_block_iterator == rhs.m_block_iterator && lhs.m_bit_index == rhs.m_bit_index;
}

template< typename Iterator >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 bool
operator!=( const bit_iterator_base< Iterator > & lhs, const bit_iterator_base< Iterator > & rhs )
{
    return ! ( lhs == rhs );
}

template< typename Iterator >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 bool
operator<( const bit_iterator_base< Iterator > & lhs, const bit_iterator_base< Iterator > & rhs )
{
    return lhs.m_block_iterator < rhs.m_block_iterator
        || ( lhs.m_block_iterator == rhs.m_block_iterator && lhs.m_bit_index < rhs.m_bit_index );
}

template< typename Iterator >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 bool
operator<=( const bit_iterator_base< Iterator > & lhs, const bit_iterator_base< Iterator > & rhs )
{
    return ! ( rhs < lhs );
}

template< typename Iterator >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 bool
operator>( const bit_iterator_base< Iterator > & lhs, const bit_iterator_base< Iterator > & rhs )
{
    return rhs < lhs;
}

template< typename Iterator >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 bool
operator>=( const bit_iterator_base< Iterator > & lhs, const bit_iterator_base< Iterator > & rhs )
{
    return ! ( lhs < rhs );
}

template< typename Iterator >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 std::ptrdiff_t
                                 operator-( const bit_iterator_base< Iterator > & lhs, const bit_iterator_base< Iterator > & rhs )
{
    return ( lhs.m_block_iterator - rhs.m_block_iterator ) * bit_iterator_base< Iterator >::bits_per_block
         + lhs.m_bit_index - rhs.m_bit_index;
}

template< typename DynamicBitset >
BOOST_DYNAMIC_BITSET_CONSTEXPR20
bit_iterator< DynamicBitset >::bit_iterator()
    : bit_iterator_base< typename DynamicBitset::buffer_type::iterator >()
{
}

template< typename DynamicBitset >
BOOST_DYNAMIC_BITSET_CONSTEXPR20
bit_iterator< DynamicBitset >::bit_iterator( typename DynamicBitset::buffer_type::iterator block_iterator, int bit_index )
    : bit_iterator_base< typename DynamicBitset::buffer_type::iterator >( block_iterator, bit_index )
{
}

template< typename DynamicBitset >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 typename DynamicBitset::reference
bit_iterator< DynamicBitset >::operator*() const
{
    return reference( *( this->m_block_iterator ), this->m_bit_index );
}

template< typename DynamicBitset >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 bit_iterator< DynamicBitset > &
                                 bit_iterator< DynamicBitset >::operator++()
{
    this->increment();
    return *this;
}
template< typename DynamicBitset >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 bit_iterator< DynamicBitset >
                                 bit_iterator< DynamicBitset >::operator++( int )
{
    bit_iterator temp = *this;
    this->increment();
    return temp;
}

template< typename DynamicBitset >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 bit_iterator< DynamicBitset > &
                                 bit_iterator< DynamicBitset >::operator--()
{
    this->decrement();
    return *this;
}

template< typename DynamicBitset >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 bit_iterator< DynamicBitset >
                                 bit_iterator< DynamicBitset >::operator--( int )
{
    bit_iterator temp = *this;
    this->decrement();
    return temp;
}

template< typename DynamicBitset >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 bit_iterator< DynamicBitset > &
                                 bit_iterator< DynamicBitset >::operator+=( difference_type n )
{
    this->add( n );
    return *this;
}

template< typename DynamicBitset >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 bit_iterator< DynamicBitset > &
                                 bit_iterator< DynamicBitset >::operator-=( difference_type n )
{
    this->add( -n );
    return *this;
}

template< typename DynamicBitset >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 bit_iterator< DynamicBitset >
                                 operator+( const bit_iterator< DynamicBitset > & it, typename bit_iterator< DynamicBitset >::difference_type n )
{
    bit_iterator< DynamicBitset > temp = it;
    temp += n;
    return temp;
}

template< typename DynamicBitset >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 bit_iterator< DynamicBitset >
                                 operator+( typename bit_iterator< DynamicBitset >::difference_type n, const bit_iterator< DynamicBitset > & it )
{
    return it + n;
}

template< typename DynamicBitset >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 bit_iterator< DynamicBitset >
                                 operator-( const bit_iterator< DynamicBitset > & it, typename bit_iterator< DynamicBitset >::difference_type n )
{
    bit_iterator< DynamicBitset > temp = it;
    temp -= n;
    return temp;
}

template< typename DynamicBitset >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 typename DynamicBitset::reference
bit_iterator< DynamicBitset >::operator[]( difference_type n ) const
{
    return *( *this + n );
}

template< typename DynamicBitset >
BOOST_DYNAMIC_BITSET_CONSTEXPR20
const_bit_iterator< DynamicBitset >::const_bit_iterator( typename DynamicBitset::buffer_type::const_iterator block_iterator, int bit_index )
    : bit_iterator_base< typename DynamicBitset::buffer_type::const_iterator >( block_iterator, bit_index )
{
}

template< typename DynamicBitset >
BOOST_DYNAMIC_BITSET_CONSTEXPR20
const_bit_iterator< DynamicBitset >::const_bit_iterator( const bit_iterator< DynamicBitset > & it )
    : bit_iterator_base< typename DynamicBitset::buffer_type::const_iterator >( it.m_block_iterator, it.m_bit_index )
{
}

template< typename DynamicBitset >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 typename const_bit_iterator< DynamicBitset >::const_reference
const_bit_iterator< DynamicBitset >::operator*() const
{
    return ( *( this->m_block_iterator ) & ( typename DynamicBitset::block_type( 1 ) << this->m_bit_index ) ) != 0;
}

template< typename DynamicBitset >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 const_bit_iterator< DynamicBitset > &
                                 const_bit_iterator< DynamicBitset >::const_bit_iterator::operator++()
{
    this->increment();
    return *this;
}

template< typename DynamicBitset >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 const_bit_iterator< DynamicBitset >
                                 const_bit_iterator< DynamicBitset >::operator++( int )
{
    const_bit_iterator temp = *this;
    this->increment();
    return temp;
}

template< typename DynamicBitset >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 const_bit_iterator< DynamicBitset > &
                                 const_bit_iterator< DynamicBitset >::const_bit_iterator::operator--()
{
    this->decrement();
    return *this;
}

template< typename DynamicBitset >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 const_bit_iterator< DynamicBitset >
                                 const_bit_iterator< DynamicBitset >::operator--( int )
{
    const_bit_iterator temp = *this;
    this->decrement();
    return temp;
}

template< typename DynamicBitset >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 const_bit_iterator< DynamicBitset > &
                                 const_bit_iterator< DynamicBitset >::operator+=( difference_type n )
{
    this->add( n );
    return *this;
}

template< typename DynamicBitset >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 const_bit_iterator< DynamicBitset > &
                                 const_bit_iterator< DynamicBitset >::operator-=( difference_type n )
{
    this->add( -n );
    return *this;
}

template< typename DynamicBitset >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 const_bit_iterator< DynamicBitset >
                                 operator+( const const_bit_iterator< DynamicBitset > & it, typename const_bit_iterator< DynamicBitset >::difference_type n )
{
    const_bit_iterator< DynamicBitset > temp = it;
    temp += n;
    return temp;
}

template< typename DynamicBitset >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 const_bit_iterator< DynamicBitset >
                                 operator+( typename const_bit_iterator< DynamicBitset >::difference_type n, const const_bit_iterator< DynamicBitset > & it )
{
    return it + n;
}

template< typename DynamicBitset >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 const_bit_iterator< DynamicBitset >
                                 operator-( const const_bit_iterator< DynamicBitset > & it, typename const_bit_iterator< DynamicBitset >::difference_type n )
{
    const_bit_iterator< DynamicBitset > temp = it;
    temp -= n;
    return temp;
}

template< typename DynamicBitset >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 typename const_bit_iterator< DynamicBitset >::const_reference
const_bit_iterator< DynamicBitset >::operator[]( difference_type n ) const
{
    return *( *this + n );
}

template< typename BlockIterator, typename B, typename A >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 void
from_block_range( BlockIterator first, BlockIterator last, dynamic_bitset< B, A > & result )
{
    // PRE: distance(first, last) <= numblocks()
    const auto it = std::copy( first, last, result.m_bits.begin() );
    if ( it == result.m_bits.end() )
    {
        result.m_zero_unused_bits();
    }
}

template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20
dynamic_bitset< Block, AllocatorOrContainer >::dynamic_bitset()
    : m_num_bits( 0 )
{
}

template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20
dynamic_bitset< Block, AllocatorOrContainer >::dynamic_bitset( const allocator_type & alloc )
    : m_bits( alloc ), m_num_bits( 0 )
{
}

template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20
dynamic_bitset< Block, AllocatorOrContainer >::
    dynamic_bitset( size_type num_bits, unsigned long value, const allocator_type & alloc )
    : m_bits( alloc ), m_num_bits( 0 )
{
    init_from_unsigned_long( num_bits, value );
}

template< typename Block, typename AllocatorOrContainer >
template< typename CharT, typename Traits, typename Alloc >
dynamic_bitset< Block, AllocatorOrContainer >::dynamic_bitset(
    const std::basic_string< CharT, Traits, Alloc > &             s,
    typename std::basic_string< CharT, Traits, Alloc >::size_type pos,
    typename std::basic_string< CharT, Traits, Alloc >::size_type n,
    size_type                                                     num_bits,
    const allocator_type &                                        alloc )

    : m_bits( alloc ), m_num_bits( 0 )
{
    init_from_string( s.c_str(), s.length(), pos, n, num_bits );
}

template< typename Block, typename AllocatorOrContainer >
template< typename CharT >
dynamic_bitset< Block, AllocatorOrContainer >::dynamic_bitset(
    const CharT *          s,
    std::size_t            n,
    size_type              num_bits,
    const allocator_type & alloc )
    : m_bits( alloc ), m_num_bits( 0 )
{
    init_from_string( s, std::char_traits< CharT >::length( s ), 0, n, num_bits );
}

#if defined( BOOST_DYNAMIC_BITSET_USE_CPP17_OR_LATER )

template< typename Block, typename AllocatorOrContainer >
template< typename CharT, typename Traits >
dynamic_bitset< Block, AllocatorOrContainer >::dynamic_bitset(
    std::basic_string_view< CharT, Traits > sv,
    size_type                               num_bits,
    const allocator_type &                  alloc )
    : m_bits( alloc ), m_num_bits( 0 )
{
    init_from_string( sv.data(), sv.length(), 0, sv.length(), num_bits );
}

#endif

template< typename Block, typename AllocatorOrContainer >
template< typename BlockInputIterator >
BOOST_DYNAMIC_BITSET_CONSTEXPR20
dynamic_bitset< Block, AllocatorOrContainer >::dynamic_bitset(
    BlockInputIterator     first,
    BlockInputIterator     last,
    const allocator_type & alloc )
    : m_bits( alloc ), m_num_bits( 0 )
{
    using boost::detail::dynamic_bitset_impl::is_numeric;
    using boost::detail::dynamic_bitset_impl::value_to_type;

    const value_to_type<
        is_numeric< BlockInputIterator >::value >
        selector;

    dispatch_init( first, last, selector );
}

// copy constructor
template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20
dynamic_bitset< Block, AllocatorOrContainer >::dynamic_bitset( const dynamic_bitset & b )
    : m_bits( b.m_bits ), m_num_bits( b.m_num_bits )
{
}

template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 dynamic_bitset< Block, AllocatorOrContainer >::~dynamic_bitset()
{
    BOOST_ASSERT( m_check_invariants() );
}

template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 typename dynamic_bitset< Block, AllocatorOrContainer >::iterator
dynamic_bitset< Block, AllocatorOrContainer >::begin()
{
    return iterator( m_bits.begin(), 0 );
}

template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 typename dynamic_bitset< Block, AllocatorOrContainer >::const_iterator
dynamic_bitset< Block, AllocatorOrContainer >::begin() const
{
    return const_iterator( m_bits.cbegin(), 0 );
}

template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 typename dynamic_bitset< Block, AllocatorOrContainer >::iterator
dynamic_bitset< Block, AllocatorOrContainer >::end()
{
    if ( count_extra_bits() == 0 ) {
        return iterator( m_bits.end(), 0 );
    } else {
        return iterator( std::prev( m_bits.end() ), size() % bits_per_block );
    }
}

template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 typename dynamic_bitset< Block, AllocatorOrContainer >::const_iterator
dynamic_bitset< Block, AllocatorOrContainer >::end() const
{
    if ( count_extra_bits() == 0 ) {
        return const_iterator( m_bits.cend(), 0 );
    } else {
        return const_iterator( std::prev( m_bits.cend() ), size() % bits_per_block );
    }
}

template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 typename dynamic_bitset< Block, AllocatorOrContainer >::reverse_iterator
dynamic_bitset< Block, AllocatorOrContainer >::rbegin()
{
    return reverse_iterator( end() );
}

template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 typename dynamic_bitset< Block, AllocatorOrContainer >::const_reverse_iterator
dynamic_bitset< Block, AllocatorOrContainer >::rbegin() const
{
    return const_reverse_iterator( end() );
}

template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 typename dynamic_bitset< Block, AllocatorOrContainer >::reverse_iterator
dynamic_bitset< Block, AllocatorOrContainer >::rend()
{
    return reverse_iterator( begin() );
}

template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 typename dynamic_bitset< Block, AllocatorOrContainer >::const_reverse_iterator
dynamic_bitset< Block, AllocatorOrContainer >::rend() const
{
    return const_reverse_iterator( begin() );
}

template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 typename dynamic_bitset< Block, AllocatorOrContainer >::const_iterator
dynamic_bitset< Block, AllocatorOrContainer >::cbegin() const
{
    return const_iterator( begin() );
}

template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 typename dynamic_bitset< Block, AllocatorOrContainer >::const_iterator
dynamic_bitset< Block, AllocatorOrContainer >::cend() const
{
    return const_iterator( end() );
}

template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 typename dynamic_bitset< Block, AllocatorOrContainer >::const_reverse_iterator
dynamic_bitset< Block, AllocatorOrContainer >::crbegin() const
{
    return const_reverse_iterator( end() );
}

template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 typename dynamic_bitset< Block, AllocatorOrContainer >::const_reverse_iterator
dynamic_bitset< Block, AllocatorOrContainer >::crend() const
{
    return const_reverse_iterator( begin() );
}

template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 void
dynamic_bitset< Block, AllocatorOrContainer >::
    swap( dynamic_bitset< Block, AllocatorOrContainer > & b ) noexcept
{
    std::swap( m_bits, b.m_bits );
    std::swap( m_num_bits, b.m_num_bits );
}

template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 dynamic_bitset< Block, AllocatorOrContainer > &
                                 dynamic_bitset< Block, AllocatorOrContainer >::
operator=( const dynamic_bitset< Block, AllocatorOrContainer > & b )
{
    m_bits     = b.m_bits;
    m_num_bits = b.m_num_bits;
    return *this;
}

template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20
dynamic_bitset< Block, AllocatorOrContainer >::
    dynamic_bitset( dynamic_bitset< Block, AllocatorOrContainer > && b )
    : m_bits( std::move( b.m_bits ) ), m_num_bits( std::move( b.m_num_bits ) )
{
    // Required so that BOOST_ASSERT(m_check_invariants()); works.
    BOOST_ASSERT( ( b.m_bits = buffer_type( get_allocator() ) ).empty() );
    b.m_num_bits = 0;
}

template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 dynamic_bitset< Block, AllocatorOrContainer > &
                                 dynamic_bitset< Block, AllocatorOrContainer >::
operator=( dynamic_bitset< Block, AllocatorOrContainer > && b )
{
    if ( &b == this ) {
        return *this;
    }

    m_bits     = std::move( b.m_bits );
    m_num_bits = std::move( b.m_num_bits );
    // Required so that BOOST_ASSERT(m_check_invariants()); works.
    BOOST_ASSERT( ( b.m_bits = buffer_type( get_allocator() ) ).empty() );
    b.m_num_bits = 0;
    return *this;
}

template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 typename dynamic_bitset< Block, AllocatorOrContainer >::allocator_type
dynamic_bitset< Block, AllocatorOrContainer >::get_allocator() const
{
    return m_bits.get_allocator();
}

//-----------------------------------------------------------------------------
// size changing operations

template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 void
dynamic_bitset< Block, AllocatorOrContainer >::
    resize( size_type num_bits, bool value ) // strong guarantee
{
    const size_type  old_num_blocks  = num_blocks();
    const size_type  required_blocks = calc_num_blocks( num_bits );

    const block_type v               = value ? Block( -1 ) : Block( 0 );

    if ( required_blocks != old_num_blocks ) {
        m_bits.resize( required_blocks, v ); // s.g. (copy)
    }

    // At this point:
    //
    //  - if the buffer was shrunk, we have nothing more to do,
    //    except a call to m_zero_unused_bits()
    //
    //  - if it was enlarged, all the (used) bits in the new blocks have
    //    the correct value, but we have not yet touched those bits, if
    //    any, that were 'unused bits' before enlarging: if value == true,
    //    they must be set.

    if ( value && ( num_bits > m_num_bits ) ) {
        const int extra_bits = count_extra_bits();
        if ( extra_bits ) {
            BOOST_ASSERT( old_num_blocks >= 1 && old_num_blocks <= m_bits.size() );

            // Set them.
            m_bits[ old_num_blocks - 1 ] |= ( v << extra_bits );
        }
    }

    m_num_bits = num_bits;
    m_zero_unused_bits();
}

template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 void
dynamic_bitset< Block, AllocatorOrContainer >::
    clear() // no throw
{
    m_bits.clear();
    m_num_bits = 0;
}

template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 void
dynamic_bitset< Block, AllocatorOrContainer >::
    push_back( bool bit )
{
    const int extra_bits = count_extra_bits();
    if ( extra_bits == 0 ) {
        m_bits.push_back( Block( bit ) );
    } else {
        m_bits.back() |= ( Block( bit ) << extra_bits );
    }
    ++m_num_bits;
}

template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 void
dynamic_bitset< Block, AllocatorOrContainer >::
    push_front( bool bit )
{
    resize( size() + 1 );
    *this <<= 1;
    set( 0, bit );
}

template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 void
dynamic_bitset< Block, AllocatorOrContainer >::
    pop_back()
{
    BOOST_ASSERT( ! empty() );

    if ( count_extra_bits() == 1 ) {
        m_bits.pop_back();
        --m_num_bits;
    } else {
        --m_num_bits;
        m_zero_unused_bits();
    }
}

template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 void
dynamic_bitset< Block, AllocatorOrContainer >::
    pop_front()
{
    BOOST_ASSERT( ! empty() );

    *this >>= 1;
    resize( size() - 1 );
}

template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 void
dynamic_bitset< Block, AllocatorOrContainer >::
    append( Block value ) // strong guarantee
{
    const int r = count_extra_bits();

    if ( r == 0 ) {
        // the buffer is empty, or all blocks are filled
        m_bits.push_back( value );
    } else {
        m_bits.push_back( value >> ( bits_per_block - r ) );
        m_bits[ m_bits.size() - 2 ] |= ( value << r ); // m_bits.size() >= 2
    }

    m_num_bits += bits_per_block;
    BOOST_ASSERT( m_check_invariants() );
}

template< typename Block, typename AllocatorOrContainer >
template< typename BlockInputIterator >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 void
dynamic_bitset< Block, AllocatorOrContainer >::append( BlockInputIterator first, BlockInputIterator last ) // strong guarantee
{
    typename std::iterator_traits< BlockInputIterator >::iterator_category cat;
    m_append( first, last, cat );
}

//-----------------------------------------------------------------------------
// bitset operations
template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 dynamic_bitset< Block, AllocatorOrContainer > &
                                 dynamic_bitset< Block, AllocatorOrContainer >::operator&=( const dynamic_bitset & rhs )
{
    BOOST_ASSERT( size() == rhs.size() );
    for ( size_type i = 0; i < num_blocks(); ++i ) {
        m_bits[ i ] &= rhs.m_bits[ i ];
    }
    return *this;
}

template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 dynamic_bitset< Block, AllocatorOrContainer > &
                                 dynamic_bitset< Block, AllocatorOrContainer >::operator|=( const dynamic_bitset & rhs )
{
    BOOST_ASSERT( size() == rhs.size() );
    for ( size_type i = 0; i < num_blocks(); ++i ) {
        m_bits[ i ] |= rhs.m_bits[ i ];
    }
    // m_zero_unused_bits();
    return *this;
}

template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 dynamic_bitset< Block, AllocatorOrContainer > &
                                 dynamic_bitset< Block, AllocatorOrContainer >::operator^=( const dynamic_bitset & rhs )
{
    BOOST_ASSERT( size() == rhs.size() );
    for ( size_type i = 0; i < this->num_blocks(); ++i ) {
        m_bits[ i ] ^= rhs.m_bits[ i ];
    }
    // m_zero_unused_bits();
    return *this;
}

template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 dynamic_bitset< Block, AllocatorOrContainer > &
                                 dynamic_bitset< Block, AllocatorOrContainer >::operator-=( const dynamic_bitset & rhs )
{
    BOOST_ASSERT( size() == rhs.size() );
    for ( size_type i = 0; i < num_blocks(); ++i ) {
        m_bits[ i ] &= ~rhs.m_bits[ i ];
    }
    // m_zero_unused_bits();
    return *this;
}

// NOTE:
//  Note that the 'if (r != 0)' is crucial to avoid undefined
//  behavior when the left hand operand of >> isn't promoted to a
//  wider type (because rs would be too large).
//
template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 dynamic_bitset< Block, AllocatorOrContainer > &
                                 dynamic_bitset< Block, AllocatorOrContainer >::operator<<=( size_type n )
{
    if ( n >= m_num_bits ) {
        return reset();
    }
    // else
    if ( n > 0 ) {
        const size_type last = num_blocks() - 1;   // num_blocks() is >= 1
        const size_type div  = n / bits_per_block; // div is <= last
        const int       r    = bit_index( n );
        buffer_type &   b    = m_bits;

        if ( r != 0 ) {
            const int rs = bits_per_block - r;

            for ( size_type i = last - div; i > 0; --i ) {
                b[ i + div ] = ( b[ i ] << r ) | ( b[ i - 1 ] >> rs );
            }
            b[ div ] = b[ 0 ] << r;

        } else {
            for ( size_type i = last - div; i > 0; --i ) {
                b[ i + div ] = b[ i ];
            }
            b[ div ] = b[ 0 ];
        }

        // zero out div blocks at the least significant end
        std::fill_n( m_bits.begin(), div, static_cast< block_type >( 0 ) );

        // zero out any 1 bit that flowed into the unused part
        m_zero_unused_bits(); // thanks to Lester Gong
    }

    return *this;
}

//
// NOTE:
//  See the comments to operator <<=.
//
template< typename B, typename A >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 dynamic_bitset< B, A > &
                                 dynamic_bitset< B, A >::operator>>=( size_type n )
{
    if ( n >= m_num_bits ) {
        return reset();
    }
    // else
    if ( n > 0 ) {
        const size_type last = num_blocks() - 1;   // num_blocks() is >= 1
        const size_type div  = n / bits_per_block; // div is <= last
        const int       r    = bit_index( n );
        buffer_type &   b    = m_bits;

        if ( r != 0 ) {
            const int ls = bits_per_block - r;

            for ( size_type i = div; i < last; ++i ) {
                b[ i - div ] = ( b[ i ] >> r ) | ( b[ i + 1 ] << ls );
            }
            // r bits go to zero
            b[ last - div ] = b[ last ] >> r;
        }

        else {
            for ( size_type i = div; i <= last; ++i ) {
                b[ i - div ] = b[ i ];
            }
            // note the '<=': the last iteration 'absorbs'
            // b[last-div] = b[last] >> 0;
        }

        // div blocks are zero filled at the most significant end
        std::fill_n( m_bits.begin() + ( num_blocks() - div ), div, static_cast< block_type >( 0 ) );
    }

    return *this;
}

template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 dynamic_bitset< Block, AllocatorOrContainer >
                                 dynamic_bitset< Block, AllocatorOrContainer >::operator<<( size_type n ) const
{
    dynamic_bitset r( *this );
    return r <<= n;
}

template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 dynamic_bitset< Block, AllocatorOrContainer >
                                 dynamic_bitset< Block, AllocatorOrContainer >::operator>>( size_type n ) const
{
    dynamic_bitset r( *this );
    return r >>= n;
}

//-----------------------------------------------------------------------------
// basic bit operations

template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 dynamic_bitset< Block, AllocatorOrContainer > &
                                 dynamic_bitset< Block, AllocatorOrContainer >::set( size_type pos, size_type len, bool val )
{
    if ( val ) {
        return range_operation( pos, len, set_block_partial, set_block_full );
    } else {
        return range_operation( pos, len, reset_block_partial, reset_block_full );
    }
}

template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 dynamic_bitset< Block, AllocatorOrContainer > &
                                 dynamic_bitset< Block, AllocatorOrContainer >::set( size_type pos, bool val )
{
    BOOST_ASSERT( pos < m_num_bits );

    if ( val ) {
        m_bits[ block_index( pos ) ] |= bit_mask( pos );
    } else {
        reset( pos );
    }

    return *this;
}

template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 dynamic_bitset< Block, AllocatorOrContainer > &
                                 dynamic_bitset< Block, AllocatorOrContainer >::set()
{
    std::fill( m_bits.begin(), m_bits.end(), Block( -1 ) );
    m_zero_unused_bits();
    return *this;
}

template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 dynamic_bitset< Block, AllocatorOrContainer > &
                                 dynamic_bitset< Block, AllocatorOrContainer >::reset( size_type pos, size_type len )
{
    return range_operation( pos, len, reset_block_partial, reset_block_full );
}

template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 dynamic_bitset< Block, AllocatorOrContainer > &
                                 dynamic_bitset< Block, AllocatorOrContainer >::reset( size_type pos )
{
    BOOST_ASSERT( pos < m_num_bits );
    m_bits[ block_index( pos ) ] &= ~bit_mask( pos );
    return *this;
}

template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 dynamic_bitset< Block, AllocatorOrContainer > &
                                 dynamic_bitset< Block, AllocatorOrContainer >::reset()
{
    std::fill( m_bits.begin(), m_bits.end(), Block( 0 ) );
    return *this;
}

template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 dynamic_bitset< Block, AllocatorOrContainer > &
                                 dynamic_bitset< Block, AllocatorOrContainer >::flip( size_type pos, size_type len )
{
    return range_operation( pos, len, flip_block_partial, flip_block_full );
}

template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 dynamic_bitset< Block, AllocatorOrContainer > &
                                 dynamic_bitset< Block, AllocatorOrContainer >::flip( size_type pos )
{
    BOOST_ASSERT( pos < m_num_bits );
    m_bits[ block_index( pos ) ] ^= bit_mask( pos );
    return *this;
}

template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 dynamic_bitset< Block, AllocatorOrContainer > &
                                 dynamic_bitset< Block, AllocatorOrContainer >::flip()
{
    for ( size_type i = 0; i < num_blocks(); ++i ) {
        m_bits[ i ] = ~m_bits[ i ];
    }
    m_zero_unused_bits();
    return *this;
}

template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 typename dynamic_bitset< Block, AllocatorOrContainer >::reference
dynamic_bitset< Block, AllocatorOrContainer >::at( size_type pos )
{
    if ( pos >= m_num_bits ) {
        BOOST_THROW_EXCEPTION( std::out_of_range( "boost::dynamic_bitset::at out_of_range" ) );
    }

    return ( *this )[ pos ];
}

template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 bool
dynamic_bitset< Block, AllocatorOrContainer >::at( size_type pos ) const
{
    if ( pos >= m_num_bits ) {
        BOOST_THROW_EXCEPTION( std::out_of_range( "boost::dynamic_bitset::at out_of_range" ) );
    }

    return ( *this )[ pos ];
}

template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 bool
dynamic_bitset< Block, AllocatorOrContainer >::test( size_type pos ) const
{
    BOOST_ASSERT( pos < m_num_bits );
    return m_unchecked_test( pos );
}

template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 bool
dynamic_bitset< Block, AllocatorOrContainer >::test_set( size_type pos, bool val )
{
    const bool b = test( pos );
    if ( b != val ) {
        set( pos, val );
    }
    return b;
}

template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 bool
dynamic_bitset< Block, AllocatorOrContainer >::all() const
{
    const int        extra_bits        = count_extra_bits();
    const block_type all_ones          = Block( -1 );
    const size_type  num_normal_blocks = num_blocks() - ( extra_bits != 0 ? 1 : 0 );

    for ( size_type i = 0; i < num_normal_blocks; ++i ) {
        if ( m_bits[ i ] != all_ones ) {
            return false;
        }
    }
    if ( extra_bits != 0 ) {
        const block_type mask = ( block_type( 1 ) << extra_bits ) - 1;
        if ( m_highest_block() != mask ) {
            return false;
        }
    }
    return true;
}

template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 bool
dynamic_bitset< Block, AllocatorOrContainer >::any() const
{
    for ( size_type i = 0; i < num_blocks(); ++i ) {
        if ( m_bits[ i ] ) {
            return true;
        }
    }
    return false;
}

template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 bool
dynamic_bitset< Block, AllocatorOrContainer >::none() const
{
    return ! any();
}

template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 dynamic_bitset< Block, AllocatorOrContainer >
                                 dynamic_bitset< Block, AllocatorOrContainer >::operator~() const
{
    dynamic_bitset b( *this );
    b.flip();
    return b;
}

template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 typename dynamic_bitset< Block, AllocatorOrContainer >::size_type
dynamic_bitset< Block, AllocatorOrContainer >::count() const noexcept
{
    size_type result = 0;
    for ( block_type block : m_bits ) {
        result += core::popcount( block );
    }
    return result;
}

//-----------------------------------------------------------------------------
// subscript

template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 typename dynamic_bitset< Block, AllocatorOrContainer >::reference
dynamic_bitset< Block, AllocatorOrContainer >::operator[]( size_type pos )
{
    return reference( m_bits[ block_index( pos ) ], bit_index( pos ) );
}

template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 bool
dynamic_bitset< Block, AllocatorOrContainer >::operator[]( size_type pos ) const
{
    return test( pos );
}

//-----------------------------------------------------------------------------
// conversions

template< typename Block, typename AllocatorOrContainer >
template< typename T >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 T
dynamic_bitset< Block, AllocatorOrContainer >::
    to_number() const
{
    // Reuse allowed_block_type to check that T is an unsigned integer type other than bool.
    static_assert( detail::dynamic_bitset_impl::allowed_block_type< T >::value, "T must be an unsigned integer type other than bool" );

    if ( m_num_bits == 0 ) {
        return 0; // convention
    }

    constexpr int t_width = std::numeric_limits< T >::digits;

    // Check for overflows. This may be a performance burden on very large
    // bitsets but is required by the specification, sorry.
    if ( find_first( t_width ) != npos ) {
        BOOST_THROW_EXCEPTION( std::overflow_error( "boost::dynamic_bitset::to_number overflow" ) );
    }

    // Ok, from now on we can be sure there's no "on" bit beyond the
    // "allowed" positions.
    const size_type maximum_size =
        (std::min)( m_num_bits, static_cast< size_type >( t_width ) );

    const size_type last_block = block_index( maximum_size - 1 );

    BOOST_ASSERT( ( last_block * bits_per_block ) < static_cast< size_type >( t_width ) );

    T result = 0;
    for ( size_type i = 0; i <= last_block; ++i ) {
        const size_type offset = i * bits_per_block;
        result |= ( static_cast< T >( m_bits[ i ] ) << offset );
    }

    return result;
}

template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 unsigned long
dynamic_bitset< Block, AllocatorOrContainer >::
    to_ulong() const
{
    return to_number< unsigned long >();
}

template< typename Block, typename AllocatorOrContainer, typename StringT >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 void
to_string( const dynamic_bitset< Block, AllocatorOrContainer > & b, StringT & s )
{
    to_string_helper( b, s, false );
}

// Differently from to_string this function dumps out every bit of the
// internal representation (may be useful for debugging purposes)
template< typename B, typename A, typename StringT >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 void
dump_to_string( const dynamic_bitset< B, A > & b, StringT & s )
{
    to_string_helper( b, s, true /* = dump_all */ );
}

template< typename Block, typename AllocatorOrContainer, typename BlockOutputIterator >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 void
to_block_range( const dynamic_bitset< Block, AllocatorOrContainer > & b, BlockOutputIterator result )
{
    // Note how this copies *all* bits, including the unused ones in the
    // last block (which are zero).
    std::copy( b.m_bits.begin(), b.m_bits.end(), result );
}

template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 typename dynamic_bitset< Block, AllocatorOrContainer >::size_type
dynamic_bitset< Block, AllocatorOrContainer >::size() const noexcept
{
    return m_num_bits;
}

template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 typename dynamic_bitset< Block, AllocatorOrContainer >::size_type
dynamic_bitset< Block, AllocatorOrContainer >::num_blocks() const noexcept
{
    return m_bits.size();
}

template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 typename dynamic_bitset< Block, AllocatorOrContainer >::size_type
dynamic_bitset< Block, AllocatorOrContainer >::max_size() const noexcept
{
    // The semantics of vector<>::max_size() aren't very clear (see lib
    // issue 197).
    //
    // Because of that, I was tempted to not provide this function
    // at all, but the user could need it if they provide their own
    // allocator.

    const size_type m = m_bits.max_size();

    return m <= ( size_type( -1 ) / bits_per_block ) ? m * bits_per_block : size_type( -1 );
}

template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 bool
dynamic_bitset< Block, AllocatorOrContainer >::empty() const noexcept
{
    return size() == 0;
}

template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 typename dynamic_bitset< Block, AllocatorOrContainer >::size_type
dynamic_bitset< Block, AllocatorOrContainer >::capacity() const noexcept
{
    return m_bits.capacity() * bits_per_block;
}

template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 void
dynamic_bitset< Block, AllocatorOrContainer >::reserve( size_type num_bits )
{
    m_bits.reserve( calc_num_blocks( num_bits ) );
}

template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 void
dynamic_bitset< Block, AllocatorOrContainer >::shrink_to_fit()
{
    if ( m_bits.size() < m_bits.capacity() ) {
        buffer_type( m_bits ).swap( m_bits );
    }
}

template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 bool
dynamic_bitset< Block, AllocatorOrContainer >::
    is_subset_of( const dynamic_bitset< Block, AllocatorOrContainer > & a ) const
{
    BOOST_ASSERT( size() == a.size() );
    for ( size_type i = 0; i < num_blocks(); ++i ) {
        if ( m_bits[ i ] & ~a.m_bits[ i ] ) {
            return false;
        }
    }
    return true;
}

template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 bool
dynamic_bitset< Block, AllocatorOrContainer >::
    is_proper_subset_of( const dynamic_bitset< Block, AllocatorOrContainer > & a ) const
{
    BOOST_ASSERT( size() == a.size() );

    bool proper = false;
    for ( size_type i = 0; i < num_blocks(); ++i ) {
        const Block & bt = m_bits[ i ];
        const Block & ba = a.m_bits[ i ];

        if ( bt & ~ba ) {
            return false; // not a subset at all
        }
        if ( ba & ~bt ) {
            proper = true;
        }
    }
    return proper;
}

template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 bool
dynamic_bitset< Block, AllocatorOrContainer >::intersects( const dynamic_bitset & b ) const
{
    const size_type common_blocks = num_blocks() < b.num_blocks()
                                      ? num_blocks()
                                      : b.num_blocks();

    for ( size_type i = 0; i < common_blocks; ++i ) {
        if ( m_bits[ i ] & b.m_bits[ i ] ) {
            return true;
        }
    }
    return false;
}

// --------------------------------
// lookup

// Look for the first bit with value `value`, starting from the block with index
// first_block.
template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 typename dynamic_bitset< Block, AllocatorOrContainer >::size_type
dynamic_bitset< Block, AllocatorOrContainer >::m_do_find_from( size_type first_block, bool value ) const
{
    size_type i = std::distance( m_bits.begin(), std::find_if( m_bits.begin() + first_block, m_bits.end(), value ? m_not_empty : m_not_full ) );

    if ( i >= num_blocks() ) {
        return npos; // not found
    }

    const Block b = value
                      ? m_bits[ i ]
                      : m_bits[ i ] ^ Block( -1 );
    return i * bits_per_block + static_cast< size_type >( detail::lowest_bit( b ) );
}

template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 typename dynamic_bitset< Block, AllocatorOrContainer >::size_type
dynamic_bitset< Block, AllocatorOrContainer >::find_first( size_type pos ) const
{
    const size_type sz = size();
    if ( pos >= sz ) {
        return npos;
    }

    const size_type blk   = block_index( pos );
    const int       ind   = bit_index( pos );

    // shift bits upto one immediately after current
    const Block     fore  = m_bits[ blk ] >> ind;

    const bool      found = m_not_empty( fore );
    return found ? pos + static_cast< size_type >( detail::lowest_bit( fore ) )
                 : m_do_find_from( blk + 1, true );
}

template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 typename dynamic_bitset< Block, AllocatorOrContainer >::size_type
dynamic_bitset< Block, AllocatorOrContainer >::find_first_off( size_type pos ) const
{
    if ( pos >= size() ) {
        return npos;
    }

    const size_type blk                = block_index( pos );
    const int       ind                = bit_index( pos );
    const Block     fore               = m_bits[ blk ] >> ind;
    bool            found              = false;
    int             lowest_off_bit_pos = -1;
    if ( m_not_full( fore ) ) {
        lowest_off_bit_pos = detail::lowest_bit( fore ^ Block( -1 ) );
        // don't consider a zero introduced by m_bits[ blk ] >> ind as found
        found              = lowest_off_bit_pos <= ( bits_per_block - 1 - ind );
    }

    const size_type zero_pos = found
                                 ? pos + lowest_off_bit_pos
                                 : m_do_find_from( blk + 1, false );
    return zero_pos >= size()
             ? npos
             : zero_pos;
}

template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 typename dynamic_bitset< Block, AllocatorOrContainer >::size_type
dynamic_bitset< Block, AllocatorOrContainer >::find_next( size_type pos ) const
{
    return pos == npos
             ? npos
             : find_first( pos + 1 );
}

template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 typename dynamic_bitset< Block, AllocatorOrContainer >::size_type
dynamic_bitset< Block, AllocatorOrContainer >::find_next_off( size_type pos ) const
{
    return pos == npos
             ? npos
             : find_first_off( pos + 1 );
}

//-----------------------------------------------------------------------------
// comparison

template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 bool
operator==( const dynamic_bitset< Block, AllocatorOrContainer > & a, const dynamic_bitset< Block, AllocatorOrContainer > & b )
{
    return ( a.m_num_bits == b.m_num_bits )
        && ( a.m_bits == b.m_bits );
}

template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 bool
operator!=( const dynamic_bitset< Block, AllocatorOrContainer > & a, const dynamic_bitset< Block, AllocatorOrContainer > & b )
{
    return ! ( a == b );
}

template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 bool
operator<( const dynamic_bitset< Block, AllocatorOrContainer > & a, const dynamic_bitset< Block, AllocatorOrContainer > & b )
{
    typedef typename dynamic_bitset< Block, AllocatorOrContainer >::size_type size_type;

    size_type                                                                 asize( a.size() );
    size_type                                                                 bsize( b.size() );

    if ( ! bsize ) {
        return false;
    } else if ( ! asize ) {
        return true;
    } else if ( asize == bsize ) {
        for ( size_type ii = a.num_blocks(); ii > 0; --ii ) {
            size_type i = ii - 1;
            if ( a.m_bits[ i ] < b.m_bits[ i ] ) {
                return true;
            } else if ( a.m_bits[ i ] > b.m_bits[ i ] ) {
                return false;
            }
        }
        return false;
    } else {
        size_type leqsize( std::min BOOST_PREVENT_MACRO_SUBSTITUTION( asize, bsize ) );

        for ( size_type ii = 0; ii < leqsize; ++ii, --asize, --bsize ) {
            size_type i = asize - 1;
            size_type j = bsize - 1;
            if ( a[ i ] < b[ j ] ) {
                return true;
            } else if ( a[ i ] > b[ j ] ) {
                return false;
            }
        }
        return a.size() < b.size();
    }
}

template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 bool
operator<=( const dynamic_bitset< Block, AllocatorOrContainer > & a, const dynamic_bitset< Block, AllocatorOrContainer > & b )
{
    return ! ( a > b );
}

template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 bool
operator>( const dynamic_bitset< Block, AllocatorOrContainer > & a, const dynamic_bitset< Block, AllocatorOrContainer > & b )
{
    return b < a;
}

template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 bool
operator>=( const dynamic_bitset< Block, AllocatorOrContainer > & a, const dynamic_bitset< Block, AllocatorOrContainer > & b )
{
    return ! ( a < b );
}

template< typename B, typename A, typename StringT >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 void
to_string_helper( const dynamic_bitset< B, A > & b, StringT & s, bool dump_all )
{
    typedef typename StringT::traits_type              Tr;
    typedef typename StringT::value_type               Ch;

    const std::ctype< Ch > &                           fac  = std::use_facet< std::ctype< Ch > >( std::locale() );
    const Ch                                           zero = fac.widen( '0' );
    const Ch                                           one  = fac.widen( '1' );

    // Note that this function may access (when
    // dump_all == true) bits beyond position size() - 1

    typedef typename dynamic_bitset< B, A >::size_type size_type;

    const size_type                                    len = dump_all ? dynamic_bitset< B, A >::bits_per_block * b.num_blocks() : b.size();
    s.assign( len, zero );

    for ( size_type i = 0; i < len; ++i ) {
        if ( b.m_unchecked_test( i ) ) {
            Tr::assign( s[ len - 1 - i ], one );
        }
    }
}

//-----------------------------------------------------------------------------
// hash operations

template< typename Block, typename AllocatorOrContainer >
std::size_t
hash_value( const dynamic_bitset< Block, AllocatorOrContainer > & a )
{
    std::size_t res = hash_value( a.m_num_bits );
    boost::hash_combine( res, a.m_bits );
    return res;
}

//-----------------------------------------------------------------------------
// stream operations

template< typename Ch, typename Tr, typename Block, typename Alloc >
std::basic_ostream< Ch, Tr > &
operator<<( std::basic_ostream< Ch, Tr > & os, const dynamic_bitset< Block, Alloc > & b )
{
    using namespace std;

    const ios_base::iostate                  ok  = ios_base::goodbit;
    ios_base::iostate                        err = ok;

    typename basic_ostream< Ch, Tr >::sentry cerberos( os );
    if ( cerberos ) {
        const Ch zero = os.widen( '0' );
        const Ch one  = os.widen( '1' );

        BOOST_TRY
        {
            typedef typename dynamic_bitset< Block, Alloc >::size_type bitset_size_type;
            typedef basic_streambuf< Ch, Tr >                          buffer_type;

            buffer_type *                                              buf         = os.rdbuf();
            // careful: os.width() is signed (and can be < 0)
            const bitset_size_type                                     width       = ( os.width() <= 0 ) ? 0 : static_cast< bitset_size_type >( os.width() );
            streamsize                                                 npad        = ( width <= b.size() ) ? 0 : width - b.size();

            const Ch                                                   fill_char   = os.fill();
            const ios_base::fmtflags                                   adjustfield = os.flags() & ios_base::adjustfield;

            // if needed fill at left; pad is decreased along the way
            if ( adjustfield != ios_base::left ) {
                for ( ; 0 < npad; --npad ) {
                    if ( Tr::eq_int_type( Tr::eof(), buf->sputc( fill_char ) ) ) {
                        err |= ios_base::failbit;
                        break;
                    }
                }
            }

            if ( err == ok ) {
                // output the bitset
                for ( bitset_size_type i = b.size(); 0 < i; --i ) {
                    typename buffer_type::int_type
                        ret = buf->sputc( b.test( i - 1 ) ? one : zero );
                    if ( Tr::eq_int_type( Tr::eof(), ret ) ) {
                        err |= ios_base::failbit;
                        break;
                    }
                }
            }

            if ( err == ok ) {
                // if needed fill at right
                for ( ; 0 < npad; --npad ) {
                    if ( Tr::eq_int_type( Tr::eof(), buf->sputc( fill_char ) ) ) {
                        err |= ios_base::failbit;
                        break;
                    }
                }
            }

            os.width( 0 );
        }
        BOOST_CATCH( ... )
        {
            bool rethrow = false;
            BOOST_TRY
            {
                os.setstate( ios_base::badbit );
            }
            BOOST_CATCH( ... )
            {
                rethrow = true;
            }
            BOOST_CATCH_END

            if ( rethrow ) {
                BOOST_RETHROW
            }
        }
        BOOST_CATCH_END
    }

    if ( err != ok ) {
        os.setstate( err ); // may throw exception
    }
    return os;
}

template< typename Ch, typename Tr, typename Block, typename Alloc >
std::basic_istream< Ch, Tr > &
operator>>( std::basic_istream< Ch, Tr > & is, dynamic_bitset< Block, Alloc > & b )
{
    using namespace std;

    typedef dynamic_bitset< Block, Alloc >   bitset_type;
    typedef typename bitset_type::size_type  size_type;

    const streamsize                         w                          = is.width();
    const size_type                          limit                      = 0 < w && static_cast< size_type >( w ) < b.max_size() ? static_cast< size_type >( w ) : b.max_size();

    bool                                     exceptions_are_from_vector = false;
    ios_base::iostate                        err                        = ios_base::goodbit;
    typename basic_istream< Ch, Tr >::sentry cerberos( is ); // skips whitespace
    if ( cerberos ) {
        // in accordance with the resolution of library issue 303
        const Ch zero = is.widen( '0' );
        const Ch one  = is.widen( '1' );

        b.clear();
        BOOST_TRY
        {
            typename bitset_type::bit_appender appender( b );
            basic_streambuf< Ch, Tr > *        buf = is.rdbuf();
            typename Tr::int_type              c   = buf->sgetc();
            for ( ; appender.get_count() < limit; c = buf->snextc() ) {
                if ( Tr::eq_int_type( Tr::eof(), c ) ) {
                    err |= ios_base::eofbit;
                    break;
                } else {
                    const Ch   to_c   = Tr::to_char_type( c );
                    const bool is_one = Tr::eq( to_c, one );

                    if ( ! is_one && ! Tr::eq( to_c, zero ) ) {
                        break; // non digit character
                    }

                    exceptions_are_from_vector = true;
                    appender.do_append( is_one );
                    exceptions_are_from_vector = false;
                }

            } // for
        }
        BOOST_CATCH( ... )
        {
            // catches from stream buf, or from vector:
            //
            // bits_stored bits have been extracted and stored, and
            // either no further character is extractable or we can't
            // append to the underlying vector (out of memory)
            if ( exceptions_are_from_vector ) {
                BOOST_RETHROW
            }

            bool rethrow = false;
            BOOST_TRY
            {
                is.setstate( ios_base::badbit );
            }
            BOOST_CATCH( ... )
            {
                rethrow = true;
            }
            BOOST_CATCH_END

            if ( rethrow ) {
                BOOST_RETHROW
            }
        }
        BOOST_CATCH_END
    }

    is.width( 0 );
    if ( b.size() == 0 /*|| !cerberos*/ ) {
        err |= ios_base::failbit;
    }
    if ( err != ios_base::goodbit ) {
        is.setstate( err ); // may throw
    }

    return is;
}

//-----------------------------------------------------------------------------
// bitset operations

template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 dynamic_bitset< Block, AllocatorOrContainer >
                                 operator&( const dynamic_bitset< Block, AllocatorOrContainer > & x, const dynamic_bitset< Block, AllocatorOrContainer > & y )
{
    dynamic_bitset< Block, AllocatorOrContainer > b( x );
    return b &= y;
}

template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 dynamic_bitset< Block, AllocatorOrContainer >
                                 operator|( const dynamic_bitset< Block, AllocatorOrContainer > & x, const dynamic_bitset< Block, AllocatorOrContainer > & y )
{
    dynamic_bitset< Block, AllocatorOrContainer > b( x );
    return b |= y;
}

template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 dynamic_bitset< Block, AllocatorOrContainer >
                                 operator^( const dynamic_bitset< Block, AllocatorOrContainer > & x, const dynamic_bitset< Block, AllocatorOrContainer > & y )
{
    dynamic_bitset< Block, AllocatorOrContainer > b( x );
    return b ^= y;
}

template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 dynamic_bitset< Block, AllocatorOrContainer >
                                 operator-( const dynamic_bitset< Block, AllocatorOrContainer > & x, const dynamic_bitset< Block, AllocatorOrContainer > & y )
{
    dynamic_bitset< Block, AllocatorOrContainer > b( x );
    return b -= y;
}

//-----------------------------------------------------------------------------
// namespace scope swap

template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 void
swap( dynamic_bitset< Block, AllocatorOrContainer > & a, dynamic_bitset< Block, AllocatorOrContainer > & b ) noexcept
{
    a.swap( b );
}

template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 bool
dynamic_bitset< Block, AllocatorOrContainer >::m_unchecked_test( size_type pos ) const
{
    return ( m_bits[ block_index( pos ) ] & bit_mask( pos ) ) != 0;
}

template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 typename dynamic_bitset< Block, AllocatorOrContainer >::size_type
dynamic_bitset< Block, AllocatorOrContainer >::calc_num_blocks( size_type num_bits )
{
    return num_bits / bits_per_block
         + static_cast< size_type >( num_bits % bits_per_block != 0 );
}

// gives a reference to the highest block
//
template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 Block &
dynamic_bitset< Block, AllocatorOrContainer >::m_highest_block()
{
    return const_cast< Block & >( static_cast< const dynamic_bitset * >( this )->m_highest_block() );
}

// gives a const-reference to the highest block
//
template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 const Block &
dynamic_bitset< Block, AllocatorOrContainer >::m_highest_block() const
{
    BOOST_ASSERT( num_blocks() > 0 );
    return m_bits.back();
}

template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 dynamic_bitset< Block, AllocatorOrContainer > &
                                 dynamic_bitset< Block, AllocatorOrContainer >::range_operation(
    size_type pos, size_type len, Block ( *partial_block_operation )( Block, size_type, size_type ), Block ( *full_block_operation )( Block ) )
{
    BOOST_ASSERT( pos + len <= m_num_bits );

    // Do nothing in case of zero length
    if ( ! len ) {
        return *this;
    }

    // Use an additional asserts in order to detect size_type overflow
    // For example: pos = 10, len = size_type_limit - 2, pos + len = 7
    // In case of overflow, 'pos + len' is always smaller than 'len'
    BOOST_ASSERT( pos + len >= len );

    // Start and end blocks of the [pos; pos + len - 1] sequence
    const size_type first_block     = block_index( pos );
    const size_type last_block      = block_index( pos + len - 1 );

    const size_type first_bit_index = bit_index( pos );
    const size_type last_bit_index  = bit_index( pos + len - 1 );

    if ( first_block == last_block ) {
        // Filling only a sub-block of a block
        m_bits[ first_block ] = partial_block_operation( m_bits[ first_block ], first_bit_index, last_bit_index );
    } else {
        // Check if the corner blocks won't be fully filled with 'val'
        const size_type first_block_shift = bit_index( pos ) ? 1 : 0;
        const size_type last_block_shift  = ( bit_index( pos + len - 1 )
                                             == bits_per_block - 1 )
                                              ? 0
                                              : 1;

        // Blocks that will be filled with ~0 or 0 at once
        const size_type first_full_block  = first_block + first_block_shift;
        const size_type last_full_block   = last_block - last_block_shift;

        for ( size_type i = first_full_block; i <= last_full_block; ++i ) {
            m_bits[ i ] = full_block_operation( m_bits[ i ] );
        }

        // Fill the first block from the 'first' bit index to the end
        if ( first_block_shift ) {
            m_bits[ first_block ] = partial_block_operation( m_bits[ first_block ], first_bit_index, bits_per_block - 1 );
        }

        // Fill the last block from the start to the 'last' bit index
        if ( last_block_shift ) {
            m_bits[ last_block ] = partial_block_operation( m_bits[ last_block ], 0, last_bit_index );
        }
    }

    return *this;
}

// If size() is not a multiple of bits_per_block then not all the bits
// in the last block are used. This function resets the unused bits
// (convenient for the implementation of many member functions).
template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 void
dynamic_bitset< Block, AllocatorOrContainer >::m_zero_unused_bits()
{
    BOOST_ASSERT( num_blocks() == calc_num_blocks( m_num_bits ) );

    // if != 0, this is the number of bits used in the last block.
    const int extra_bits = count_extra_bits();

    if ( extra_bits != 0 ) {
        m_highest_block() &= ( Block( 1 ) << extra_bits ) - 1;
    }
}

// check class invariants
template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 bool
dynamic_bitset< Block, AllocatorOrContainer >::m_check_invariants() const
{
    const int extra_bits = count_extra_bits();
    if ( extra_bits > 0 ) {
        const block_type mask = Block( -1 ) << extra_bits;
        if ( ( m_highest_block() & mask ) != 0 ) {
            return false;
        }
    }
    if ( m_bits.size() > m_bits.capacity() || num_blocks() != calc_num_blocks( size() ) ) {
        return false;
    }

    return true;
}

template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 bool
dynamic_bitset< Block, AllocatorOrContainer >::m_not_empty( Block x )
{
    return x != Block( 0 );
}

template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 bool
dynamic_bitset< Block, AllocatorOrContainer >::m_not_full( Block x )
{
    return x != Block( -1 );
}

template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 int
dynamic_bitset< Block, AllocatorOrContainer >::count_extra_bits() const noexcept
{
    return bit_index( size() );
}

template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 typename dynamic_bitset< Block, AllocatorOrContainer >::size_type
dynamic_bitset< Block, AllocatorOrContainer >::block_index( size_type pos ) noexcept
{
    return pos / bits_per_block;
}

template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 int
dynamic_bitset< Block, AllocatorOrContainer >::bit_index( size_type pos ) noexcept
{
    return static_cast< int >( pos % bits_per_block );
}

template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 Block
dynamic_bitset< Block, AllocatorOrContainer >::bit_mask( size_type pos ) noexcept
{
    return Block( 1 ) << bit_index( pos );
}

template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 Block
dynamic_bitset< Block, AllocatorOrContainer >::bit_mask( size_type first, size_type last ) noexcept
{
    Block res = ( last == bits_per_block - 1 )
                  ? Block( -1 )
                  : ( ( Block( 1 ) << ( last + 1 ) ) - 1 );
    res ^= ( Block( 1 ) << first ) - 1;
    return res;
}

template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 Block
dynamic_bitset< Block, AllocatorOrContainer >::set_block_bits(
    Block     block,
    size_type first,
    size_type last,
    bool      val ) noexcept
{
    if ( val ) {
        return block | bit_mask( first, last );
    } else {
        return block & static_cast< Block >( ~bit_mask( first, last ) );
    }
}

// Functions for operations on ranges
template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 Block
dynamic_bitset< Block, AllocatorOrContainer >::set_block_partial(
    Block     block,
    size_type first,
    size_type last ) noexcept
{
    return set_block_bits( block, first, last, true );
}

template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 Block
dynamic_bitset< Block, AllocatorOrContainer >::set_block_full( Block ) noexcept
{
    return Block( -1 );
}

template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 Block
dynamic_bitset< Block, AllocatorOrContainer >::reset_block_partial(
    Block     block,
    size_type first,
    size_type last ) noexcept
{
    return set_block_bits( block, first, last, false );
}

template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 Block
dynamic_bitset< Block, AllocatorOrContainer >::reset_block_full( Block ) noexcept
{
    return 0;
}

template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 Block
dynamic_bitset< Block, AllocatorOrContainer >::flip_block_partial(
    Block     block,
    size_type first,
    size_type last ) noexcept
{
    return block ^ bit_mask( first, last );
}

template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 Block
dynamic_bitset< Block, AllocatorOrContainer >::flip_block_full( Block block ) noexcept
{
    return ~block;
}

template< typename Block, typename AllocatorOrContainer >
template< typename T >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 void
dynamic_bitset< Block, AllocatorOrContainer >::dispatch_init(
    T             num_bits,
    unsigned long value,
    detail::dynamic_bitset_impl::value_to_type< true > )
{
    init_from_unsigned_long( static_cast< size_type >( num_bits ), value );
}

template< typename Block, typename AllocatorOrContainer >
template< typename T >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 void
dynamic_bitset< Block, AllocatorOrContainer >::dispatch_init(
    T first,
    T last,
    detail::dynamic_bitset_impl::value_to_type< false > )
{
    init_from_block_range( first, last );
}

template< typename Block, typename AllocatorOrContainer >
template< typename BlockIter >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 void
dynamic_bitset< Block, AllocatorOrContainer >::init_from_block_range( BlockIter first, BlockIter last )
{
    BOOST_ASSERT( m_bits.size() == 0 );
    m_bits.insert( m_bits.end(), first, last );
    m_num_bits = m_bits.size() * bits_per_block;
}

template< typename Block, typename AllocatorOrContainer >
template< typename CharT, typename Traits >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 void
dynamic_bitset< Block, AllocatorOrContainer >::init_from_string(
    const CharT * s, // caution: not necessarily null-terminated
    std::size_t   string_length,
    std::size_t   pos,
    std::size_t   n,
    size_type     num_bits )
{
    BOOST_ASSERT( pos <= string_length );

    const std::size_t rlen = (std::min)( n, string_length - pos );
    const size_type   sz   = ( num_bits != npos ? num_bits : rlen );
    m_bits.resize( calc_num_blocks( sz ) );
    m_num_bits                      = sz;

    const std::ctype< CharT > & fac = std::use_facet< std::ctype< CharT > >( std::locale() );
    const CharT                 one = fac.widen( '1' );

    const size_type             m   = num_bits < rlen ? num_bits : rlen;
    for ( std::size_t i = 0; i < m; ++i ) {
        const CharT c = s[ ( pos + m - 1 ) - i ];

        if ( Traits::eq( c, one ) ) {
            set( i );
        } else {
            BOOST_ASSERT( Traits::eq( c, fac.widen( '0' ) ) );
        }
    }
}

template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 void
dynamic_bitset< Block, AllocatorOrContainer >::init_from_unsigned_long(
    size_type     num_bits,
    unsigned long value )
{
    BOOST_ASSERT( m_bits.size() == 0 );

    m_bits.resize( calc_num_blocks( num_bits ) );
    m_num_bits = num_bits;

    typedef unsigned long                                                                        num_type;
    typedef boost::detail::dynamic_bitset_impl::shifter< num_type, bits_per_block, ulong_width > shifter;

    // if (num_bits == 0)
    //     return;

    // zero out all bits at pos >= num_bits, if any;
    // note that: num_bits == 0 implies value == 0
    if ( num_bits < static_cast< size_type >( ulong_width ) ) {
        const num_type mask = ( num_type( 1 ) << num_bits ) - 1;
        value &= mask;
    }

    typename buffer_type::iterator it = m_bits.begin();
    for ( ; value; shifter::right_shift( value ), ++it ) {
        *it = static_cast< block_type >( value );
    }
}

template< typename Block, typename AllocatorOrContainer >
template< typename BlockInputIterator >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 void
dynamic_bitset< Block, AllocatorOrContainer >::m_append( BlockInputIterator first, BlockInputIterator last, std::input_iterator_tag )
{
    for ( ; first != last; ++first ) {
        append( *first );
    }
}

template< typename Block, typename AllocatorOrContainer >
template< typename BlockInputIterator >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 void
dynamic_bitset< Block, AllocatorOrContainer >::m_append( BlockInputIterator first, BlockInputIterator last, std::forward_iterator_tag )
{
    if ( first != last ) {
        const int         r = count_extra_bits();
        const std::size_t d = std::distance( first, last );
        m_bits.reserve( num_blocks() + d );
        if ( r == 0 ) {
            do {
                m_bits.push_back( *first ); // could use vector<>::insert()
                ++first;
            } while ( first != last );
        } else {
            m_highest_block() |= ( *first << r );
            do {
                Block b = *first >> ( bits_per_block - r );
                ++first;
                m_bits.push_back( b | ( first == last ? 0 : *first << r ) );
            } while ( first != last );
        }
        m_num_bits += bits_per_block * d;
    }
}

// bit appender

template< typename Block, typename AllocatorOrContainer >
dynamic_bitset< Block, AllocatorOrContainer >::bit_appender::bit_appender( dynamic_bitset & r )
    : bs( r ), n( 0 ), mask( 0 ), current( 0 )
{
}

template< typename Block, typename AllocatorOrContainer >
dynamic_bitset< Block, AllocatorOrContainer >::bit_appender::~bit_appender()
{
    // Reverse the order of the blocks, shift if needed, and then
    // resize.
    //
    std::reverse( bs.m_bits.begin(), bs.m_bits.end() );
    const int offs = bit_index( n );
    if ( offs ) {
        bs >>= ( bits_per_block - offs );
    }
    bs.resize( n ); // doesn't enlarge, so can't throw
    BOOST_ASSERT( bs.m_check_invariants() );
}

template< typename Block, typename AllocatorOrContainer >
void
dynamic_bitset< Block, AllocatorOrContainer >::bit_appender::do_append( bool value )
{
    if ( mask == 0 ) {
        bs.append( Block( 0 ) );
        current = &bs.m_highest_block();
        mask    = Block( 1 ) << ( bits_per_block - 1 );
    }

    if ( value ) {
        *current |= mask;
    }
    mask /= 2;
    ++n;
}

template< typename Block, typename AllocatorOrContainer >
typename dynamic_bitset< Block, AllocatorOrContainer >::size_type
dynamic_bitset< Block, AllocatorOrContainer >::bit_appender::get_count() const
{
    return n;
}

} // namespace boost

// std::hash support
#if defined( BOOST_DYNAMIC_BITSET_SPECIALIZE_STD_HASH )
namespace std {

template< typename Block, typename AllocatorOrContainer >
struct hash< boost::dynamic_bitset< Block, AllocatorOrContainer > >
{
    typedef boost::dynamic_bitset< Block, AllocatorOrContainer > argument_type;
    typedef std::size_t                                          result_type;
    result_type
    operator()( const argument_type & a ) const noexcept
    {
        boost::hash< argument_type > hasher;
        return hasher( a );
    }
};

}
#endif
