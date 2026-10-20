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

#ifndef BOOST_DYNAMIC_BITSET_DYNAMIC_BITSET_HPP
#define BOOST_DYNAMIC_BITSET_DYNAMIC_BITSET_HPP

#include "boost/dynamic_bitset/config.hpp"
#include "boost/dynamic_bitset/detail/dynamic_bitset.hpp"
#include "boost/dynamic_bitset_fwd.hpp"
#include <iosfwd>
#include <iterator>
#include <limits>
#include <string>
#include <type_traits>
#include <vector>

#if defined( BOOST_DYNAMIC_BITSET_SPECIALIZE_STD_HASH )
#    include <functional>
namespace std {

//!     Support for std::hash.
//!
//!     You can exclude this support by defining the macro
//!     `BOOST_DYNAMIC_BITSET_NO_STD_HASH`.
// -----------------------------------------------------------------------
template< typename Block, typename AllocatorOrContainer >
struct hash< boost::dynamic_bitset< Block, AllocatorOrContainer > >;

}
#endif

namespace boost {

template< typename Iterator >
class bit_iterator_base;

template< typename DynamicBitset >
class bit_iterator;

template< typename DynamicBitset >
class const_bit_iterator;

//!     The `dynamic_bitset` template represents a set of bits.
//!
//!     \par Template parameters
//!     - `Block`
//!       The integer type in which the bits are stored. Defaults to
//!       `unsigned long`.
//!
//!     - `AllocatorOrContainer`
//!       Either an allocator (to use for all internal memory management) or
//!       a container of Block's. Defaults to `std::allocator< Block >`.
//!
//!     \par Concepts modeled
//!     DefaultConstructible, CopyConstructible, CopyAssignable,
//!     MoveConstructible, MoveAssignable, EqualityComparable,
//!     LessThanComparable.
//!
//!     \par Type requirements
//!     `Block` is a cv-unqualified unsigned integer type other than
//!     `bool`. `AllocatorOrContainer` satisfies the standard requirements for an
//!     <a href="https://en.cppreference.com/w/cpp/named_req/Allocator.html">allocator</a>
//!     or is a container-like type which provides at least bidirectional
//!     iterators.
// ---------------------------------------------------------------------------
template< typename Block, typename AllocatorOrContainer >
class dynamic_bitset
{
    static_assert( (bool)detail::dynamic_bitset_impl::allowed_block_type< Block >::value, "Block type not allowed" );
    static_assert( std::is_same< Block, typename AllocatorOrContainer::value_type >::value, "Block is not the same type as AllocatorOrContainer::value_type" );

public:
    //!     The same type as `Block`.
    // -----------------------------------------------------------------------
    typedef Block block_type;

    //!     The allocator used for all memory allocations.
    // -----------------------------------------------------------------------
    typedef typename detail::dynamic_bitset_impl::allocator_type_extractor< AllocatorOrContainer, Block >::type
                        allocator_type;

    //!     An unsigned integral type that can represent the size of the
    //!     bitset. See \ref size().
    // -----------------------------------------------------------------------
    typedef std::size_t size_type;

    //!     Note: Made public to cope with failures from many GCC and
    //!     Clang versions which seem to ignore the friend declarations
    //!     of `bit_iterator` and `const_bit_iterator`.
    // -----------------------------------------------------------------------
    typedef typename std::conditional<
        detail::dynamic_bitset_impl::is_container< AllocatorOrContainer, Block >::value,
        AllocatorOrContainer,
        std::vector< Block, AllocatorOrContainer > >::type buffer_type;

    //!     The number of bits the type `Block` uses to represent
    //!     values, excluding any padding bits. Numerically equal to
    //!     `std::numeric_limits< Block >::digits`.
    // -----------------------------------------------------------------------
    static constexpr int                                   bits_per_block = std::numeric_limits< Block >::digits;

    //!     The maximum value of `size_type`.
    // -----------------------------------------------------------------------
    static constexpr size_type                             npos           = static_cast< size_type >( -1 );

    //!     A proxy class to simulate lvalues of bit type.
    //!
    //!     This class exists only as a helper class for
    //!     `dynamic_bitset`'s `operator[]`. The following list
    //!     describes the valid operations on the reference type. Assume
    //!     that `b` is an instance of `dynamic_bitset`, `i`, `j` are of
    //!     `size_type` and in the range `[0, b.size())`. Also, note
    //!     that when we write `b[ i ]` we mean an object of type
    //!     `reference` that was initialized from `b[ i ]`. The variable
    //!     `x` is a `bool`.
    //!
    //!     - `(bool)b[ i ]`
    //!
    //!       Returns the i-th bit of `b`.
    //!
    //!     - `(bool)~ b[ i ]`
    //!
    //!       Returns the opposite of the i-th bit of `b`.
    //!
    //!     - `b[ i ].flip()`
    //!
    //!       Flips the i-th bit of `b` and returns `b[ i ]`.
    //!
    //!     - `x = b[ i ]`
    //!
    //!       Assigns the i-th bit of `b` to `x`.
    //!
    //!     - `b[ i ] = x`
    //!
    //!       Sets the i-th bit of `b` to the value of `x` and returns
    //!       `b[ i ]`.
    //!
    //!     - `b[ i ] = b[ j ]`
    //!
    //!       Sets the i-th bit of `b` to the value of the j-th bit of
    //!       `b` and returns `b[ i ]`.
    //!
    //!     - `b[ i ] |= x`
    //!
    //!       Does an OR of the i-th bit of `b` with the value of `x`
    //!       and returns `b[ i ]`.
    //!
    //!     - `b[ i ] &= x`
    //!
    //!       Does an AND of the i-th bit of `b` with the value of `x`
    //!       and returns `b[ i ]`.
    //!
    //!     - `b[ i ] ^= x`
    //!
    //!       Does a XOR of the i-th bit of `b` with the value of `x`
    //!       and returns `b[ i ]`.
    //!
    //!     - `b[ i ] -= x`
    //!
    //!       If `x` is `true`, clears the i-th bit of `b`. Returns `b[
    //!       i ]`.
    //!
    //!     - `b[ i ] |= b[ j ]`
    //!
    //!       Does an OR of the i-th bit of `b` with the j-th bit of `b`
    //!       and returns `b[ i ]`.
    //!
    //!     - `b[ i ] &= b[ j ]`
    //!
    //!       Does an AND of the i-th bit of `b` with the j-th bit of
    //!       `b` and returns `b[ i ]`.
    //!
    //!     - `b[ i ] ^= b[ j ]`
    //!
    //!       Does a XOR of the i-th bit of `b` with the j-th bit of `b`
    //!       and returns `b[ i ]`.
    //!
    //!     - `b[ i ] -= b[ j ]`
    //!
    //!       If the j-th bit of `b` is set, clears the i-th bit of `b`.
    //!       Returns `b[ i ]`.
    // -----------------------------------------------------------------------
    class reference
    {
        friend class dynamic_bitset< Block, AllocatorOrContainer >;
        friend class bit_iterator< dynamic_bitset >;

        //!     The one and only non-copy ctor
        // -------------------------------------------------------------------
        BOOST_DYNAMIC_BITSET_CONSTEXPR20 reference( block_type & b, int pos );

    public:
        //!     Deleted address-of operator.
        // -------------------------------------------------------------------
        void                                         operator&() = delete;

        //!     Copy constructor.
        //!
        //!     Constructs a `reference` which refers to the same bit as
        //!     `other`.
        // -------------------------------------------------------------------
        BOOST_DYNAMIC_BITSET_CONSTEXPR20             reference( const reference & other );

        //!     See the class description.
        // -------------------------------------------------------------------
        BOOST_DYNAMIC_BITSET_CONSTEXPR20             operator bool() const;

        //!     See the class description.
        //!
        //!     \return The opposite of the value of `*this`.
        // -------------------------------------------------------------------
        BOOST_DYNAMIC_BITSET_CONSTEXPR20 bool        operator~() const;

        //!     See the class description.
        // -------------------------------------------------------------------
        BOOST_DYNAMIC_BITSET_CONSTEXPR20 reference & flip();

        //!     See the class description.
        // -------------------------------------------------------------------
        BOOST_DYNAMIC_BITSET_CONSTEXPR20 reference & operator=( bool x );

        //!     See the class description.
        // -------------------------------------------------------------------
        BOOST_DYNAMIC_BITSET_CONSTEXPR20 reference & operator=( const reference & rhs );

        //!     See the class description.
        // -------------------------------------------------------------------
        BOOST_DYNAMIC_BITSET_CONSTEXPR20 reference & operator|=( bool x );

        //!     See the class description.
        // -------------------------------------------------------------------
        BOOST_DYNAMIC_BITSET_CONSTEXPR20 reference & operator&=( bool x );

        //!     See the class description.
        // -------------------------------------------------------------------
        BOOST_DYNAMIC_BITSET_CONSTEXPR20 reference & operator^=( bool x );

        //!     See the class description.
        // -------------------------------------------------------------------
        BOOST_DYNAMIC_BITSET_CONSTEXPR20 reference & operator-=( bool x );

    private:
        block_type &                          m_block;
        const block_type                      m_mask;

        BOOST_DYNAMIC_BITSET_CONSTEXPR20 void do_set();
        BOOST_DYNAMIC_BITSET_CONSTEXPR20 void do_reset();
        BOOST_DYNAMIC_BITSET_CONSTEXPR20 void do_flip();
        BOOST_DYNAMIC_BITSET_CONSTEXPR20 void do_assign( bool x );
    };

    //!     The type bool.
    // -----------------------------------------------------------------------
    typedef bool const_reference;

    friend class bit_iterator< dynamic_bitset >;
    friend class const_bit_iterator< dynamic_bitset >;

    //!     A read/write iterator into the bitset.
    //!
    //!     If `AllocatorOrContainer` is an allocator type, this is a
    //!     C++20 RandomAccessIterator; otherwise, its category is the
    //!     corresponding "non-legacy" category of the iterator type of
    //!     the underlying container; for instance, if the underlying
    //!     container provides LegacyBidirectionalIterators, this is a
    //!     BidirectionalIterator.
    // -----------------------------------------------------------------------
    typedef bit_iterator< dynamic_bitset >          iterator;

    //!     A read-only iterator into the bitset.
    //!
    //!     \copydetails iterator
    // -----------------------------------------------------------------------
    typedef const_bit_iterator< dynamic_bitset >    const_iterator;

    //!     A reverse read/write reverse iterator into the bitset.
    // -----------------------------------------------------------------------
    typedef std::reverse_iterator< iterator >       reverse_iterator;

    //!     A reverse read-only iterator into the bitset.
    // -----------------------------------------------------------------------
    typedef std::reverse_iterator< const_iterator > const_reverse_iterator;

#if defined( __cpp_lib_ranges )
    static_assert( std::bidirectional_iterator< typename buffer_type::iterator >, "AllocatorOrContainer doesn't provide at least BidirectionalIterators" );
    static_assert( std::bidirectional_iterator< iterator > );
#endif

    //!     Constructs a bitset of size zero.
    //!
    //!     \post
    //!     `this->size() == 0`.
    //!
    //!     (Required by <a href="https://en.cppreference.com/w/cpp/named_req/DefaultConstructible">DefaultConstructible</a>.)
    // -----------------------------------------------------------------------
    BOOST_DYNAMIC_BITSET_CONSTEXPR20          dynamic_bitset();

    //!     Constructs a bitset of size zero.
    //!
    //!     \param alloc An allocator, a copy of which will be used to
    //!     allocate memory when needed.
    //!
    //!     \post
    //!     `this->size() == 0`
    // -----------------------------------------------------------------------
    explicit BOOST_DYNAMIC_BITSET_CONSTEXPR20 dynamic_bitset( const allocator_type & alloc );

    //!     Constructs a bitset from an integer.
    //!
    //!     The first `M` bits (where `M = min( num_bits,
    //!     std::numeric_limits< unsigned long >::digits )`) are
    //!     initialized to the corresponding bits in `value` and all
    //!     other bits, if any, to zero. A copy of the `alloc` object
    //!     will be used in subsequent bitset operations such as
    //!     `resize()` to allocate memory. Note that, e.g., the
    //!     following
    //!
    //!     \code
    //!     dynamic_bitset b<>( 16, 7 );
    //!     \endcode
    //!
    //!     will match the constructor from an iterator range (not this
    //!     one), but the underlying implementation will still "do the
    //!     right thing" and construct a bitset of 16 bits, from the
    //!     value 7.
    //!
    //!     \param num_bits The size of the constructed bitset.
    //!     \param value The value to initialize the bitset from.
    //!     \param alloc The allocator to use.
    //!
    //!     \post
    //!     - `this->size() == num_bits`
    //!     - For all i in the range `[0, M)`, `( *this )[ i ] == (
    //!       value >> i ) & 1`.
    //!     - For all i in the range `[M, num_bits)`, `( *this )[ i ] ==
    //!       false`.
    //!
    //!     \par Throws
    //!     An allocation error if memory is exhausted (`std::bad_alloc`
    //!     if `allocator_type` is a `std::allocator`).
    // -----------------------------------------------------------------------
    explicit BOOST_DYNAMIC_BITSET_CONSTEXPR20 dynamic_bitset( size_type num_bits, unsigned long value = 0, const allocator_type & alloc = allocator_type() );

    //!     Constructs a bitset from a string of 0's and 1's.
    //!
    //!     The size of the bitset is `num_bits` if `num_bits != npos`,
    //!     otherwise `rlen = min( n, s.size() - pos )`. The first `M =
    //!     min( num_bits, rlen )` bits are initialized to the
    //!     corresponding characters in `s`. Note that the highest
    //!     character position in `s`, not the lowest, corresponds to
    //!     the least significant bit. So, for example, `dynamic_bitset(
    //!     std::string( "1101" ))` is the same as `dynamic_bitset(
    //!     13ul)`.
    //!
    //!     \pre
    //!     `pos <= s.size()` and the characters used to initialize the
    //!     bits compare equal to either `std::use_facet< std::ctype< CharT > >( std::locale() ).widen( '0' )`
    //!     or `std::use_facet< std::ctype< CharT > >( std::locale() ).widen( '1' )`. E.g.:
    //!     `dynamic_bitset<> b( std::string( "10xyz" ), 0, 2 ); // OK`.

    //!
    //!     \param s The string to construct from.
    //!     \param pos The start position in the string.
    //!     \param n The maximum number of characters in the string to
    //!     consider.
    //!     \param num_bits The size of the bitset to construct, if
    //!     different from `npos`.
    //!     \param alloc The allocator to use.
    // -----------------------------------------------------------------------
    template< typename CharT, typename Traits, typename Alloc >
    explicit dynamic_bitset( const std::basic_string< CharT, Traits, Alloc > & s, typename std::basic_string< CharT, Traits, Alloc >::size_type pos = 0, typename std::basic_string< CharT, Traits, Alloc >::size_type n = ( std::basic_string< CharT, Traits, Alloc >::npos ), size_type num_bits = npos, const allocator_type & alloc = allocator_type() );

    //!     Similar to the constructor from a `basic_string`, but takes
    //!     a pointer to a C-style string (and doesn't take a `pos`).
    //!
    //!     The size of the bitset is `num_bits` if `num_bits != npos`,
    //!     otherwise `rlen = min( n, std::char_traits< CharT >::length( s ) )`.
    //!     The first `M = min( num_bits, rlen )` bits are initialized
    //!     to the corresponding characters in `s`.
    //!
    //!     \pre
    //!     The characters in `s` that are used to initialize the bits
    //!     compare equal to either `std::use_facet< std::ctype< CharT > >( std::locale() ).widen( '0' )`
    //!     or `std::use_facet< std::ctype< CharT > >( std::locale() ).widen( '1' )`. E.g.:
    //!     `dynamic_bitset<> b( "10xyz", 2 ); // OK`.
    //!
    //!     \param s The string to construct from.
    //!     \param n The maximum number of characters in the string to
    //!     consider.
    //!     \param num_bits The size of the bitset to construct, if
    //!     different from `npos`.
    //!     \param alloc The allocator to use.
    // -----------------------------------------------------------------------
    template< typename CharT >
    explicit dynamic_bitset( const CharT * s, std::size_t n = std::size_t( -1 ), size_type num_bits = npos, const allocator_type & alloc = allocator_type() );

#if defined( BOOST_DYNAMIC_BITSET_USE_CPP17_OR_LATER )

    //!     Similar to the constructor from a pointer to a C-style
    //!     string, but takes a `std::basic_string_view`. This
    //!     constructor is only available if DynamicBitset is compiled
    //!     as C++17 or later.
    //!
    //!     \pre
    //!     The characters in `sv` that are use to initialize the bits
    //!     compare equal to either `std::use_facet< std::ctype< CharT > >( std::locale() ).widen( '0' )`
    //!     or `std::use_facet< std::ctype< CharT > >( std::locale() ).widen( '1' )`. E.g.:
    //!     `dynamic_bitset<> b( std::string_view( "10xyz", 2 ) ); // OK`.
    //!
    //!     \param sv The basic_string_view to construct from.
    //!     \param num_bits The size of the bitset to construct, if
    //!     different from `npos`. (Otherwise the size of the bitset is
    //!     `sv.length()`.)
    //!     \param alloc The allocator to use.
    // -----------------------------------------------------------------------
    template< typename CharT, typename Traits >
    explicit dynamic_bitset( std::basic_string_view< CharT, Traits > sv, size_type num_bits = npos, const allocator_type & alloc = allocator_type() );

#endif

    //!     Constructs a bitset from a range of blocks or from an
    //!     integer.
    //!
    //!     If this constructor is called with a type
    //!     `BlockInputIterator` which is actually an integral type, the
    //!     library behaves as if the constructor from `unsigned long`
    //!     were called, with arguments `static_cast< size_type >( first )`,
    //!     `last` and `alloc`, in that order.
    //!
    //!     \par Example
    //!     Given:
    //!
    //!     \code
    //!     dynamic_bitset< unsigned short > b( 8, 7 );
    //!     \endcode
    //!
    //!     `b` is constructed as if by calling the constructor:
    //!
    //!     \code
    //!     dynamic_bitset(size_type num_bits,
    //!                    unsigned long value = 0,
    //!                    const allocator_type & alloc = allocator_type())
    //!     \endcode
    //!
    //!     with arguments:
    //!
    //!     \code
    //!     static_cast< dynamic_bitset< unsigned short >::size_type >( 8 ),
    //!     7,
    //!     allocator_type()
    //!     \endcode
    //!
    //!     Note:
    //!     At the time of writing (October 2008) this is aligned with
    //!     the proposed resolution for library issue 438. That is a
    //!     post C++03 change, and is currently in the working paper for
    //!     C++0x. Informally speaking, the critical changes with
    //!     respect to C++03 are the drop of a `static_cast` on the
    //!     second argument, and more leeway as to when the templated
    //!     constructor should have the same effect as the `(size, value)`
    //!     one: Only when `InputIterator` is an integral type, in
    //!     C++03; when it is either an integral type or any other type
    //!     that the implementation might detect as impossible to be an
    //!     input iterator, with the proposed resolution. For the
    //!     purposes of dynamic_bitset we limit ourselves to the first
    //!     of these two changes.
    //!
    //!     Otherwise (i.e. if the template argument is not an integral
    //!     type), constructs a bitset based on a range of blocks. Let
    //!     `*first` be block number 0, `\*++first` block number 1, etc.
    //!     Block number `b` is used to initialize the bits of the
    //!     dynamic_bitset in the position range `[b * bits_per_block, (
    //!     b + 1 ) * bits_per_block)`. For each block number `b` with
    //!     value `bval`, the bit `( bval >> i ) & 1` corresponds to the
    //!     bit at position `b * bits_per_block + i` in the bitset
    //!     (where i goes through the range `[0, bits_per_block)`).
    //!     \pre
    //!     `BlockInputIterator` must be either an integral type or a
    //!     model of <a href="https://en.cppreference.com/w/cpp/named_req/InputIterator">LegacyInputIterator</a>
    //!     whose `value_type` is the same type as `Block`.
    //!
    //!     \param first `numbits` if the template argument is an
    //!     integral type, otherwise the start of the range.
    //!     \param last `value` if the template argument is an integral
    //!     type, otherwise the end of the range.
    //!     \param alloc The allocator to use.
    //!
    //!     \par Throws
    //!     An allocation error if memory is exhausted (`std::bad_alloc`
    //!     if `allocator_type` is a `std::allocator`).
    // -----------------------------------------------------------------------
    template< typename BlockInputIterator >
    BOOST_DYNAMIC_BITSET_CONSTEXPR20                  dynamic_bitset( BlockInputIterator first, BlockInputIterator last, const allocator_type & alloc = allocator_type() );

    //!     Copy constructor.
    //!
    //!     Constructs a bitset that is a copy of the bitset `b`. The
    //!     allocator for this bitset is a copy of the allocator of `b`.
    //!
    //!     \post
    //!     For all i in the range `[0, b.size())`, `( *this )[ i ] ==
    //!     b[ i ]`.
    //!
    //!     \par Throws
    //!     An allocation error if memory is exhausted (`std::bad_alloc`
    //!     if `allocator_type` is a `std::allocator`).
    //!
    //!     (Required by <a href="https://en.cppreference.com/w/cpp/named_req/CopyConstructible">CopyConstructible</a>.)
    // -----------------------------------------------------------------------
    BOOST_DYNAMIC_BITSET_CONSTEXPR20                  dynamic_bitset( const dynamic_bitset & b );

    //!     Copy assignment operator.
    //!
    //!     This bitset becomes a copy of the bitset `b`.
    //!
    //!     \post
    //!     For all `i` in the range `[0, x.size())`, `( *this )[ i ] ==
    //!     b[ i ]`.
    //!
    //!     \return
    //!     `*this`.
    //!
    //!     \par Throws
    //!     An allocation error if memory is exhausted (`std::bad_alloc`
    //!     if `allocator_type` is a `std::allocator`).
    //!     (Required by <a href="https://en.cppreference.com/w/cpp/named_req/CopyAssignable">CopyAssignable</a>.)
    // -----------------------------------------------------------------------
    BOOST_DYNAMIC_BITSET_CONSTEXPR20 dynamic_bitset & operator=( const dynamic_bitset & b );

    //!     Destructor.
    //!
    //!     Releases the memory associated with this bitset and destroys
    //!     the bitset object itself.
    // -----------------------------------------------------------------------
    BOOST_DYNAMIC_BITSET_CONSTEXPR20 ~dynamic_bitset();

    //!     Returns a read/write iterator that refers to the least
    //!     significant bit in the bitset.
    // -----------------------------------------------------------------------
    BOOST_DYNAMIC_BITSET_CONSTEXPR20 iterator               begin();

    //!     Returns a read-only iterator that refers to the least
    //!     significant bit in the bitset.
    // -----------------------------------------------------------------------
    BOOST_DYNAMIC_BITSET_CONSTEXPR20 const_iterator         begin() const;

    //!     Returns a read/write iterator that refers one past the most
    //!     significant bit in the bitset.
    // -----------------------------------------------------------------------
    BOOST_DYNAMIC_BITSET_CONSTEXPR20 iterator               end();

    //!     Returns a read-only iterator that refers one past the most
    //!     significant bit in the bitset.
    // -----------------------------------------------------------------------
    BOOST_DYNAMIC_BITSET_CONSTEXPR20 const_iterator         end() const;

    //!     Returns a read/write reverse iterator that refers to the
    //!     most significant bit in the bitset.
    // -----------------------------------------------------------------------
    BOOST_DYNAMIC_BITSET_CONSTEXPR20 reverse_iterator       rbegin();

    //!     Returns a read-only reverse iterator that refers to the most
    //!     significant bit in the bitset.
    // -----------------------------------------------------------------------
    BOOST_DYNAMIC_BITSET_CONSTEXPR20 const_reverse_iterator rbegin() const;

    //!     Returns a read/write reverse iterator that refers to one
    //!     before the least significant bit in the bitset.
    // -----------------------------------------------------------------------
    BOOST_DYNAMIC_BITSET_CONSTEXPR20 reverse_iterator       rend();

    //!     Returns a read-only reverse iterator that refers to one
    //!     before the least significant bit in the bitset.
    // -----------------------------------------------------------------------
    BOOST_DYNAMIC_BITSET_CONSTEXPR20 const_reverse_iterator rend() const;

    //!     Returns a read-only iterator that refers to the least
    //!     significant bit in the bitset.
    // -----------------------------------------------------------------------
    BOOST_DYNAMIC_BITSET_CONSTEXPR20 const_iterator         cbegin() const;

    //!     Returns a read-only iterator that refers to one past the
    //!     most significant bit in the bitset.
    // -----------------------------------------------------------------------
    BOOST_DYNAMIC_BITSET_CONSTEXPR20 const_iterator         cend() const;

    //!     Returns a read-only reverse iterator that refers to the most
    //!     significant bit in the bitset.
    // -----------------------------------------------------------------------
    BOOST_DYNAMIC_BITSET_CONSTEXPR20 const_reverse_iterator crbegin() const;

    //!     Returns a read-only reverse iterator that refers to one
    //!     before the least significant bit in the bitset.
    // -----------------------------------------------------------------------
    BOOST_DYNAMIC_BITSET_CONSTEXPR20 const_reverse_iterator crend() const;

    //!     Swaps the contents of this bitset and bitset `b`.
    //!
    //!     \param b The bitset to be swapped with `*this`.
    //!
    //!     \par Throws
    //!     Nothing.
    // -----------------------------------------------------------------------
    BOOST_DYNAMIC_BITSET_CONSTEXPR20 void                   swap( dynamic_bitset & b ) noexcept;

    //!     Move constructor.
    //!
    //!     Constructs a bitset that is the same as the bitset `src`,
    //!     while using the resources from `src`. The allocator for this
    //!     bitset is moved from the allocator in `src`.
    //!
    //!     \par Throws
    //!     An allocation error if memory is exhausted (`std::bad_alloc`
    //!     if `allocator_type` is a `std::allocator`).
    // -----------------------------------------------------------------------
    BOOST_DYNAMIC_BITSET_CONSTEXPR20                        dynamic_bitset( dynamic_bitset && src );

    //!     Move assignment operator.
    //!
    //!     This bitset becomes the same as the bitset `src`, while
    //!     using the resources from `src`.
    //!
    //!     \return
    //!     `*this`.
    //!
    //!     \par Throws
    //!     Nothing.
    // -----------------------------------------------------------------------
    BOOST_DYNAMIC_BITSET_CONSTEXPR20 dynamic_bitset &       operator=( dynamic_bitset && src );

    //!     Returns a copy of the allocator object used to construct
    //!     `*this`.
    //!
    //!     \return A copy of the said allocator.
    // -----------------------------------------------------------------------
    BOOST_DYNAMIC_BITSET_CONSTEXPR20 allocator_type         get_allocator() const;

    //!     Changes the number of bits of the bitset to `num_bits`.
    //!
    //!     If `num_bits >= size()` then the bits in the range `[0,
    //!     size())` remain the same, and the bits in `[size(), num_bits)`
    //!     are all set to `value`. If `num_bits < size()` then the bits
    //!     in the range `[0, num_bits)` stay the same (and the
    //!     remaining bits are discarded).
    //!
    //!     \param num_bits The new size of the bitset.
    //!     \param value The value to set any new bit to.
    // -----------------------------------------------------------------------
    BOOST_DYNAMIC_BITSET_CONSTEXPR20 void                   resize( size_type num_bits, bool value = false );

    //!     Clears the bitset, i.e. makes its size zero.
    //!
    //!     \par Throws
    //!     Nothing.
    // -----------------------------------------------------------------------
    BOOST_DYNAMIC_BITSET_CONSTEXPR20 void                   clear();

    //!     Increases the size of the bitset by one, and sets the value
    //!     of the new most significant bit to `bit`.
    //!
    //!     \par Throws
    //!     An allocation error if memory is exhausted (`std::bad_alloc`
    //!     if `allocator_type` is a `std::allocator`).
    //!
    //!     \param bit The value to set the most significant bit to.
    // -----------------------------------------------------------------------
    BOOST_DYNAMIC_BITSET_CONSTEXPR20 void                   push_back( bool bit );

    //!     Increases the size of the bitset by one, and sets the value
    //!     of the new least significant bit to `bit`.
    //!
    //!     \par Throws
    //!     An allocation error if memory is exhausted (`std::bad_alloc`
    //!     if `allocator_type` is a `std::allocator`).
    //!
    //!     \param bit The value to set the least significant bit to.
    // -----------------------------------------------------------------------
    BOOST_DYNAMIC_BITSET_CONSTEXPR20 void                   push_front( bool bit );

    //!     Decreases the size of the bitset by one, removing the most
    //!     significant bit.
    //!
    //!     \pre
    //!     `! this->empty()`.
    // -----------------------------------------------------------------------
    BOOST_DYNAMIC_BITSET_CONSTEXPR20 void                   pop_back();

    //!     Decreases the size of the bitset by one, removing the least
    //!     significant bit.
    //!
    //!     \pre
    //!     `! this->empty()`.
    // -----------------------------------------------------------------------
    BOOST_DYNAMIC_BITSET_CONSTEXPR20 void                   pop_front();

    //!     Appends the bits in `block` to this bitset (appends to the
    //!     most significant end). This increases the size of the bitset
    //!     by `bits_per_block`. Let `s` be the old size of the bitset,
    //!     then for `i` in the range `[0, bits_per_block)`, the bit at
    //!     position `s + i` is set to `( block >> i ) & 1`.
    //!
    //!     \param block The block to append.
    // -----------------------------------------------------------------------
    BOOST_DYNAMIC_BITSET_CONSTEXPR20 void                   append( Block block );

    //!     Appends a range of blocks to `*this`.
    //!
    //!     This member provides the same end result as the following
    //!     code, but is typically more efficient.
    //!
    //!     \code
    //!     for (; first != last; ++first) {
    //!         append( *first );
    //!     }
    //!     \endcode
    //!
    //!     \pre
    //!     The `BlockInputIterator` type must be a model of
    //!     <a href="https://en.cppreference.com/w/cpp/named_req/InputIterator">LegacyInputIterator</a>
    //!     and its value_type must be the same type as Block.
    //!
    //!     \param first The start of the range.
    //!     \param last The end of the range.
    // -----------------------------------------------------------------------
    template< typename BlockInputIterator >
    BOOST_DYNAMIC_BITSET_CONSTEXPR20 void             append( BlockInputIterator first, BlockInputIterator last ); // strong guarantee

    //!     Bitwise-ANDs all the bits in this bitset with the bits in
    //!     `b`.
    //!
    //!     This is equivalent to:
    //!     \code
    //!     for ( size_type i = 0; i != this->size(); ++ i ) {
    //!         ( *this )[ i ] = ( *this )[ i ] & b[ i ];
    //!     }
    //!     \endcode
    //!
    //!     \pre
    //!     `this->size() == b.size()`.
    //!
    //!     \return
    //!     `*this`.
    // -----------------------------------------------------------------------
    BOOST_DYNAMIC_BITSET_CONSTEXPR20 dynamic_bitset & operator&=( const dynamic_bitset & b );

    //!     Bitwise-ORs all the bits in this bitset with the bits in
    //!     `b`.
    //!
    //!     This is equivalent to:
    //!     \code
    //!     for ( size_type i = 0; i != this->size(); ++ i ) {
    //!         ( *this )[ i ] = ( *this )[ i ] | b[ i ];
    //!     }
    //!     \endcode
    //!
    //!     \pre
    //!     `this->size() == b.size()`.
    //!
    //!     \return
    //!     `*this`.
    // -----------------------------------------------------------------------
    BOOST_DYNAMIC_BITSET_CONSTEXPR20 dynamic_bitset & operator|=( const dynamic_bitset & b );

    //!     Bitwise-XORs all the bits in this bitset with the bits in
    //!     `b`.
    //!
    //!     This is equivalent to:
    //!     \code
    //!     for ( size_type i = 0; i != this->size(); ++ i ) {
    //!         ( *this )[ i ] = ( *this )[ i ] ^ b[ i ];
    //!     }
    //!     \endcode
    //!
    //!     \pre
    //!     `this->size() == b.size()`.
    //!
    //!     \return
    //!     `*this`.
    // -----------------------------------------------------------------------
    BOOST_DYNAMIC_BITSET_CONSTEXPR20 dynamic_bitset & operator^=( const dynamic_bitset & b );

    //!     Computes the set difference of this bitset and the `b`
    //!     bitset.
    //!
    //!     This is equivalent to:
    //!     \code
    //!     for ( size_type i = 0; i != this->size(); ++ i ) {
    //!         ( *this )[ i ] = ( *this )[ i ] && ! b[ i ];
    //!     }
    //!     \endcode
    //!
    //!     \pre
    //!     `this->size() == b.size()`.
    //!
    //!     \return
    //!     `*this`.
    // -----------------------------------------------------------------------
    BOOST_DYNAMIC_BITSET_CONSTEXPR20 dynamic_bitset & operator-=( const dynamic_bitset & b );

    //!     Shifts the bits in this bitset to the left by `n` positions.
    //!
    //!     For each bit in the bitset, the bit at position `pos` takes
    //!     on the previous value of the bit at position `pos - n`, or
    //!     zero if no such bit exists.
    //!
    //!     \return
    //!     `*this`.
    //!
    //!     \par Throws
    //!     Nothing.
    // -----------------------------------------------------------------------
    BOOST_DYNAMIC_BITSET_CONSTEXPR20 dynamic_bitset & operator<<=( size_type n );

    //!     Shifts the bits in this bitset to the right by `n`
    //!     positions.
    //!
    //!     For each bit in the bitset, the bit at position `pos` takes
    //!     on the previous value of the bit at position `pos + n`, or
    //!     zero if no such bit exists.
    //!
    //!     \return
    //!     `*this`.
    //!
    //!     \par Throws
    //!     Nothing.
    // -----------------------------------------------------------------------
    BOOST_DYNAMIC_BITSET_CONSTEXPR20 dynamic_bitset & operator>>=( size_type n );

    //!     Returns a shifted copy of `*this`.
    //!
    //!     \return
    //!     A copy of `*this` shifted to the left by `n` positions. For
    //!     each bit in the returned bitset, the bit at position `pos`
    //!     takes on the value of the bit at position `pos - n` of this
    //!     bitset, or zero if no such bit exists.
    //!
    //!     \par Throws
    //!     An allocation error if memory is exhausted (`std::bad_alloc`
    //!     if `allocator_type` is a `std::allocator`).
    // -----------------------------------------------------------------------
    BOOST_DYNAMIC_BITSET_CONSTEXPR20 dynamic_bitset   operator<<( size_type n ) const;

    //!     Returns a shifted copy of `*this`.
    //!
    //!     \return
    //!     A copy of `*this` shifted to the right by `n` positions. For
    //!     each bit in the returned bitset, the bit at position `pos`
    //!     takes on the value of the bit at position `pos + n` of this
    //!     bitset, or zero if no such bit exists.
    //!
    //!     \par Throws
    //!     An allocation error if memory is exhausted (`std::bad_alloc`
    //!     if `allocator_type` is a `std::allocator`).
    // -----------------------------------------------------------------------
    BOOST_DYNAMIC_BITSET_CONSTEXPR20 dynamic_bitset   operator>>( size_type n ) const;

    //!     Sets the bits in the range `[pos, pos + len)` to `val`.
    //!
    //!     If `len` is zero, does nothing. Otherwise, sets all the bits
    //!     in this bitset which have a position in `[pos, pos + len -
    //!     1]` to `val`.
    //!
    //!     \pre
    //!     `pos + len <= this->size()`.
    //!
    //!     \param pos The position of the first bit to set.
    //!     \param len The number of bits to set.
    //!     \param val The value to set the bits to.
    //!
    //!     \return
    //!     `*this`.
    // -----------------------------------------------------------------------
    BOOST_DYNAMIC_BITSET_CONSTEXPR20 dynamic_bitset & set( size_type pos, size_type len, bool val /* = true */ ); // default would make it ambiguous

    //!     Sets the bit at position `pos` in this bitset to `val`.
    //!
    //!     \pre
    //!     `pos < this->size()`.
    //!
    //!     \param pos The position of the bit to set or clear.
    //!     \param val The value to set the bit to.
    //!
    //!     \return
    //!     `*this`.
    // -----------------------------------------------------------------------
    BOOST_DYNAMIC_BITSET_CONSTEXPR20 dynamic_bitset & set( size_type pos, bool val = true );

    //!     Sets all the bits in this bitset.
    //!
    //!     \return
    //!     `*this`.
    //!
    //!     \par Throws
    //!     Nothing.
    // -----------------------------------------------------------------------
    BOOST_DYNAMIC_BITSET_CONSTEXPR20 dynamic_bitset & set();

    //!     If `len` is zero, does nothing. Otherwise, resets all the
    //!     bits in this bitset which have a position in `[pos, pos +
    //!     len - 1]`.
    //!
    //!     \pre
    //!     `pos + len <= this->size()`.
    //!
    //!     \param pos The position of the lowest bit to reset.
    //!     \param len The number of bits to reset.
    //!
    //!     \return
    //!     `*this`.
    // -----------------------------------------------------------------------
    BOOST_DYNAMIC_BITSET_CONSTEXPR20 dynamic_bitset & reset( size_type pos, size_type len );

    //!     Resets the bit in this bitset at position `pos`.
    //!
    //!     \pre
    //!     `pos < this->size()`.
    //!
    //!     \param pos The position of the bit to reset.
    //!
    //!     \return
    //!     `this`.
    // -----------------------------------------------------------------------
    BOOST_DYNAMIC_BITSET_CONSTEXPR20 dynamic_bitset & reset( size_type pos );

    //!     Resets all the bits in this bitset.
    //!
    //!     \return
    //!     `*this`.
    // -----------------------------------------------------------------------
    BOOST_DYNAMIC_BITSET_CONSTEXPR20 dynamic_bitset & reset();

    //!     Toggles the bits in the range `[pos, pos + len)`.
    //!
    //!     If `len` is zero, does nothing. Otherwise, toggles all the
    //!     bits in this bitset which have a position in `[pos, pos +
    //!     len - 1]`.
    //!
    //!     \pre
    //!     `pos + len <= this->size()`.
    //!
    //!     \param pos The position of the lowest bit to toggle.
    //!     \param len The number of bits to toggle.
    //!
    //!     \return
    //!     `*this`.
    // -----------------------------------------------------------------------
    BOOST_DYNAMIC_BITSET_CONSTEXPR20 dynamic_bitset & flip( size_type pos, size_type len );

    //!     Toggles the bit at position `pos` in this bitset.
    //!
    //!     \pre
    //!     `pos < this->size()`.
    //!
    //!     \param pos The position of the bit to toggle.
    //!
    //!     \return
    //!     `*this`.
    // -----------------------------------------------------------------------
    BOOST_DYNAMIC_BITSET_CONSTEXPR20 dynamic_bitset & flip( size_type pos );

    //!     Toggles the value of every bit in this bitset.
    //!
    //!     \return
    //!     `*this`.
    //!
    //!     \par Throws
    //!     Nothing.
    // -----------------------------------------------------------------------
    BOOST_DYNAMIC_BITSET_CONSTEXPR20 dynamic_bitset & flip();

    //!     A checked version of `operator[]()`.
    //!
    //!     \param pos The position of the bit to test.
    //!
    //!     \return
    //!     The same as `operator[]( pos )`.
    //!
    //!     \par Throws
    //!     `std::out_of_range` if `pos` is not within the range of the
    //!     bitset.
    // -----------------------------------------------------------------------
    BOOST_DYNAMIC_BITSET_CONSTEXPR20 reference        at( size_type pos );

    //!     A checked version of `operator[]()`.
    //!
    //!     \param pos The position of the bit to test.
    //!
    //!     \return
    //!     The same as `operator[]( pos )`.
    //!
    //!     \par Throws
    //!     `std::out_of_range` if `pos` is not within the range of the
    //!     bitset.
    // -----------------------------------------------------------------------
    BOOST_DYNAMIC_BITSET_CONSTEXPR20 bool             at( size_type pos ) const;

    //!     Tests the bit at the given position.
    //!
    //!     \pre
    //!     `pos < this->size()`.
    //!
    //!     \param pos The position of the bit to test.
    //!
    //!     \return
    //!     `true` if bit `pos` is set, and `false` if it is zero.
    // -----------------------------------------------------------------------
    BOOST_DYNAMIC_BITSET_CONSTEXPR20 bool             test( size_type pos ) const;

    //!     Sets bit `pos` if `val` is `true`, and clears it if `val` is
    //!     `false`.
    //!
    //!     \pre
    //!     `pos < this->size()`.
    //!
    //!     \param pos The position of the bit to set or clear.
    //!     \param val The value to set the bit at position `pos` to.
    //!
    //!     \return
    //!     The previous state of bit `pos`.
    // -----------------------------------------------------------------------
    BOOST_DYNAMIC_BITSET_CONSTEXPR20 bool             test_set( size_type pos, bool val = true );

    //!     Checks whether all bits in `*this` are set.
    //!
    //!     \return
    //!     `true` if all bits in this bitset are set or if `size() ==
    //!     0`; otherwise `false`.
    //!
    //!     \par Throws
    //!     Nothing.
    // -----------------------------------------------------------------------
    BOOST_DYNAMIC_BITSET_CONSTEXPR20 bool             all() const;

    //!     Checks whether any bits in `*this` are set.
    //!
    //!     \return
    //!     `true` if any bits in this bitset are set, otherwise
    //!     `false`.
    //!
    //!     \par Throws
    //!     Nothing.
    // -----------------------------------------------------------------------
    BOOST_DYNAMIC_BITSET_CONSTEXPR20 bool             any() const;

    //!     Checks whether this bitset has no set bit.
    //!
    //!     \return
    //!     `true` if no bits in this bitset are set, otherwise `false`.
    // -----------------------------------------------------------------------
    BOOST_DYNAMIC_BITSET_CONSTEXPR20 bool             none() const;

    //!     Returns a copy of `*this` with all of its bits toggled.
    //!
    //!     \return A copy of `*this` with all of its bits toggled.
    //!
    //!     \par Throws
    //!     An allocation error if memory is exhausted (`std::bad_alloc`
    //!     if `allocator_type` is a `std::allocator`).
    // -----------------------------------------------------------------------
    BOOST_DYNAMIC_BITSET_CONSTEXPR20 dynamic_bitset   operator~() const;

    //!     Returns the number of bits in this bitset that are set.
    //!
    //!     \par Throws
    //!     Nothing.
    // -----------------------------------------------------------------------
    BOOST_DYNAMIC_BITSET_CONSTEXPR20 size_type        count() const noexcept;

    //!     Returns a `reference` to the bit at position `pos`.
    //!
    //!     \pre
    //!     `pos < this->size()`.
    //!
    //!     \return
    //!     A `reference` to bit `pos`. Note that `reference` is a proxy
    //!     class with an assignment operator and a conversion to
    //!     `bool`, which allows you to use `operator[]` for assignment.
    //!     That is, you can write both `x = b[ n ]` and `b[ n ] = x`.
    //!     However, in many other respects the proxy is not the same as
    //!     the true reference type `bool &`.
    // -----------------------------------------------------------------------
    BOOST_DYNAMIC_BITSET_CONSTEXPR20 reference        operator[]( size_type pos );

    //!     The same as `test( pos )`.
    //!
    //!     \pre
    //!     `pos < this->size()`.
    //!
    //!     \return
    //!     The same as `test( pos )`.
    // -----------------------------------------------------------------------
    BOOST_DYNAMIC_BITSET_CONSTEXPR20 bool             operator[]( size_type pos ) const;

    //!     Returns the numeric value corresponding to the bits in
    //!     `*this` (zero if `*this` is empty).
    //!
    //!     \par Type requirements
    //!     `T` is a cv-unqualified unsigned integer type other than
    //!     `bool`.
    //!
    //!     \par Throws
    //!     `std::overflow_error` if that value is too large to be
    //!     represented in `T`, i.e. if `*this` has any non-zero bit at a
    //!     position >= `std::numeric_limits< T >::digits`.
    //!
    //!     \return
    //!     The numeric value corresponding to the bits in `*this`.
    // -----------------------------------------------------------------------
    template< typename T >
    BOOST_DYNAMIC_BITSET_CONSTEXPR20 T               to_number() const;

    //!     Returns the numeric value corresponding to the bits in
    //!     `*this` (zero if `*this` is empty).
    //!
    //!     Equivalent to `to_number< unsigned long >()`.
    //!
    //!     \par Throws
    //!     `std::overflow_error` if that value is too large to be
    //!     represented in an `unsigned long`, i.e. if `*this` has any
    //!     non-zero bit at a position >= `std::numeric_limits< unsigned
    //!     long >::digits`.
    //!
    //!     \return
    //!     The numeric value corresponding to the bits in `*this`.
    // -----------------------------------------------------------------------
    BOOST_DYNAMIC_BITSET_CONSTEXPR20 unsigned long    to_ulong() const;

    //!     Returns the number of bits in this bitset.
    //!
    //!     \par Throws
    //!     Nothing.
    // -----------------------------------------------------------------------
    BOOST_DYNAMIC_BITSET_CONSTEXPR20 size_type        size() const noexcept;

    //!     Returns the number of blocks in this bitset.
    //!
    //!     \par Throws
    //!     Nothing.
    // -----------------------------------------------------------------------
    BOOST_DYNAMIC_BITSET_CONSTEXPR20 size_type        num_blocks() const noexcept;

    //!     Returns the maximum size of a bitset of this type.
    //!
    //!     \par Throws
    //!     Nothing.
    //!
    //!     \return
    //!     The maximum size of a `dynamic_bitset` object having the
    //!     same type as `*this`. Note that if any `dynamic_bitset`
    //!     operation causes `size()` to exceed `max_size()` then
    //!     <em>the behavior is undefined</em>.
    //!
    //!     [The semantics of this function could change slightly when
    //!     lib issue 197 will be closed.]
    // -----------------------------------------------------------------------
    BOOST_DYNAMIC_BITSET_CONSTEXPR20 size_type        max_size() const noexcept;

    //!     Checks whether this bitset has size zero.
    //!
    //!     \return
    //!     `this->size() == 0`.
    //!
    //!     \par Note
    //!     Not to be confused with `none()`, which has different
    //!     semantics.
    //!
    //!     \par Throws
    //!     Nothing.
    // -----------------------------------------------------------------------
    BOOST_DYNAMIC_BITSET_CONSTEXPR20 bool             empty() const noexcept;

    //!     Returns the total number of elements that `*this` can hold
    //!     without requiring reallocation.
    //!
    //!     \return The abovementioned number of elements.
    //!
    //!     \par Throws
    //!     Nothing.
    // -----------------------------------------------------------------------
    BOOST_DYNAMIC_BITSET_CONSTEXPR20 size_type        capacity() const noexcept;

    //!     Informs the bitset of a planned change in size, so that it
    //!     can manage the storage allocation accordingly.
    //!
    //!     After `reserve()`, `capacity()` is greater or equal to the
    //!     argument of `reserve()` if reallocation happens; and equal
    //!     to the previous value of `capacity()` otherwise.
    //!     Reallocation happens at this point if and only if the
    //!     current capacity is less than the argument of `reserve()`.
    //!
    //!     \param num_bits The number of bits the bitset should be able
    //!     to store without reallocation.
    //!
    //!     \par Note
    //!     It does not change the size of the bitset.
    // -----------------------------------------------------------------------
    BOOST_DYNAMIC_BITSET_CONSTEXPR20 void             reserve( size_type num_bits );

    //!     Requests the bitset to reduce memory use by removing unused
    //!     capacity.
    //!
    //!     \par Note
    //!     It does not change the size of the bitset.
    //!
    //!     \par Throws
    //!     An allocation error if memory is exhausted (`std::bad_alloc`
    //!     if `allocator_type` is a `std::allocator`).
    // -----------------------------------------------------------------------
    BOOST_DYNAMIC_BITSET_CONSTEXPR20 void             shrink_to_fit();

    //!     Checks whether `*this` is a subset of `b`.
    //!
    //!     \pre
    //!     `this->size() == b.size()`.
    //!
    //!     \param b The bitset to test `*this` against.
    //!
    //!     \return
    //!     `true` if this bitset is a subset of bitset `b`. That is, it
    //!     returns `true` if, for every bit that is set in this bitset,
    //!     the corresponding bit in bitset `b` is also set. Otherwise
    //!     this function returns `false`.
    // -----------------------------------------------------------------------
    BOOST_DYNAMIC_BITSET_CONSTEXPR20 bool             is_subset_of( const dynamic_bitset & b ) const;

    //!     Checks whether `*this` is a proper subset of `b`.
    //!
    //!     \pre
    //!     `this->size() == b.size()`.
    //!
    //!     \param b The bitset to test `*this` against.
    //!
    //!     \return
    //!     `true` if this bitset is a proper subset of bitset `b`. That
    //!     is, it returns `true` if, for every bit that is set in this
    //!     bitset, the corresponding bit in bitset a is also set and if
    //!     `this->count() < b.count()`. Otherwise this function returns
    //!     `false`.
    // -----------------------------------------------------------------------
    BOOST_DYNAMIC_BITSET_CONSTEXPR20 bool             is_proper_subset_of( const dynamic_bitset & b ) const;

    //!     Checks whether `*this` intersects with `b`.
    //!
    //!     \pre
    //!     `this->size() == b.size()`.
    //!
    //!     \param b The bitset to test `*this` against.
    //!
    //!     \return
    //!     `true` if this bitset and `b` intersect. That is, it returns
    //!     `true` if there is a bit which is set in this bitset, such
    //!     that the corresponding bit in bitset `b` is also set.
    //!     Otherwise this function returns `false`.
    // -----------------------------------------------------------------------
    BOOST_DYNAMIC_BITSET_CONSTEXPR20 bool             intersects( const dynamic_bitset & b ) const;

    //!     Finds the first set bit in `*this` with an index >= `pos`,
    //!     if any.
    //!
    //!     \return
    //!     The lowest index `i` greater than or equal to `pos` such
    //!     that bit `i` is set in `*this`, or `npos` if no such index
    //!     exists.
    //!
    //!     \par Throws
    //!     Nothing.
    // -----------------------------------------------------------------------
    BOOST_DYNAMIC_BITSET_CONSTEXPR20 size_type        find_first( size_type pos = 0 ) const;

    //!     Finds the first unset bit in `*this` with an index >= `pos`,
    //!     if any.
    //!
    //!     \param pos The lower bound (inclusively) to start the search
    //!     from.
    //!
    //!     \return
    //!     The lowest index `i` greater than or equal to `pos` such
    //!     that bit `i` is unset in `*this`, or `npos` if no such index
    //!     exists.
    //!
    //!     \par Throws
    //!     Nothing.
    // -----------------------------------------------------------------------
    BOOST_DYNAMIC_BITSET_CONSTEXPR20 size_type        find_first_off( size_type pos = 0 ) const;

    //!     Finds the first bit set in `*this` with an index > `pos`, if
    //!     any.
    //!
    //!     \param pos The lower bound (exclusively) to start the search
    //!     from.
    //!
    //!     \return
    //!     The lowest index `i` greater than `pos` such that bit `i` is
    //!     set, or `npos` if no such index exists.
    //!
    //!     \par Throws
    //!     Nothing.
    // -----------------------------------------------------------------------
    BOOST_DYNAMIC_BITSET_CONSTEXPR20 size_type        find_next( size_type pos ) const;

    //!     Finds the first unset bit in `*this` with an index > `pos`,
    //!     if any.
    //!
    //!     \param pos The lower bound (exclusively) to start the search
    //!     from.
    //!
    //!     \return
    //!     The lowest index `i` greater than `pos` such that bit `i` is
    //!     unset, or `npos` if no such index exists.
    // -----------------------------------------------------------------------
    BOOST_DYNAMIC_BITSET_CONSTEXPR20 size_type        find_next_off( size_type pos ) const;

    template< typename B, typename A >
    friend BOOST_DYNAMIC_BITSET_CONSTEXPR20 bool operator==( const dynamic_bitset< B, A > & a, const dynamic_bitset< B, A > & b );

    template< typename B, typename A >
    friend BOOST_DYNAMIC_BITSET_CONSTEXPR20 bool operator<( const dynamic_bitset< B, A > & a, const dynamic_bitset< B, A > & b );

    template< typename B, typename A, typename BlockOutputIterator >
    friend BOOST_DYNAMIC_BITSET_CONSTEXPR20 void to_block_range( const dynamic_bitset< B, A > & b, BlockOutputIterator result );

    template< typename BlockIterator, typename B, typename A >
    friend BOOST_DYNAMIC_BITSET_CONSTEXPR20 void from_block_range( BlockIterator first, BlockIterator last, dynamic_bitset< B, A > & result );

    template< typename CharT, typename Traits, typename B, typename A >
    friend std::basic_istream< CharT, Traits > & operator>>( std::basic_istream< CharT, Traits > & is, dynamic_bitset< B, A > & b );

    template< typename B, typename A, typename StringT >
    friend BOOST_DYNAMIC_BITSET_CONSTEXPR20 void to_string_helper( const dynamic_bitset< B, A > & b, StringT & s, bool dump_all );

    //!     Computes a hash value for a `dynamic_bitset`.
    //!
    //!     This enables the use of `dynamic_bitset` in hash-based
    //!     containers such as `boost::unordered_map` or
    //!     `boost::unordered_set`.
    //!
    //!     \return The computed hash value.
    // -----------------------------------------------------------------------
    template< typename B, typename A >
    friend std::size_t hash_value( const dynamic_bitset< B, A > & a );

    //!     Optional zero-copy serialization support.
    // -----------------------------------------------------------------------
    class serialize_impl;
    friend class serialize_impl;

private:
    static constexpr int                              ulong_width = std::numeric_limits< unsigned long >::digits;

    BOOST_DYNAMIC_BITSET_CONSTEXPR20 dynamic_bitset & range_operation( size_type pos, size_type len, Block ( *partial_block_operation )( Block, size_type, size_type ), Block ( *full_block_operation )( Block ) );
    BOOST_DYNAMIC_BITSET_CONSTEXPR20 void             m_zero_unused_bits();
    BOOST_DYNAMIC_BITSET_CONSTEXPR20 bool             m_check_invariants() const;

    BOOST_DYNAMIC_BITSET_CONSTEXPR20 static bool      m_not_empty( Block x );
    BOOST_DYNAMIC_BITSET_CONSTEXPR20 static bool      m_not_full( Block x );
    BOOST_DYNAMIC_BITSET_CONSTEXPR20 size_type        m_do_find_from( size_type first_block, bool value ) const;

    BOOST_DYNAMIC_BITSET_CONSTEXPR20 int              count_extra_bits() const noexcept;
    BOOST_DYNAMIC_BITSET_CONSTEXPR20 static size_type block_index( size_type pos ) noexcept;
    BOOST_DYNAMIC_BITSET_CONSTEXPR20 static int       bit_index( size_type pos ) noexcept;
    BOOST_DYNAMIC_BITSET_CONSTEXPR20 static Block     bit_mask( size_type pos ) noexcept;
    BOOST_DYNAMIC_BITSET_CONSTEXPR20 static Block     bit_mask( size_type first, size_type last ) noexcept;
    BOOST_DYNAMIC_BITSET_CONSTEXPR20 static Block     set_block_bits( Block block, size_type first, size_type last, bool val ) noexcept;

    // Functions for operations on ranges
    BOOST_DYNAMIC_BITSET_CONSTEXPR20 static Block     set_block_partial( Block block, size_type first, size_type last ) noexcept;
    BOOST_DYNAMIC_BITSET_CONSTEXPR20 static Block     set_block_full( Block ) noexcept;
    BOOST_DYNAMIC_BITSET_CONSTEXPR20 static Block     reset_block_partial( Block block, size_type first, size_type last ) noexcept;
    BOOST_DYNAMIC_BITSET_CONSTEXPR20 static Block     reset_block_full( Block ) noexcept;
    BOOST_DYNAMIC_BITSET_CONSTEXPR20 static Block     flip_block_partial( Block block, size_type first, size_type last ) noexcept;
    BOOST_DYNAMIC_BITSET_CONSTEXPR20 static Block     flip_block_full( Block block ) noexcept;

    template< typename T >
    BOOST_DYNAMIC_BITSET_CONSTEXPR20 void dispatch_init( T num_bits, unsigned long value, detail::dynamic_bitset_impl::value_to_type< true > );

    template< typename T >
    BOOST_DYNAMIC_BITSET_CONSTEXPR20 void dispatch_init( T first, T last, detail::dynamic_bitset_impl::value_to_type< false > );

    template< typename BlockIter >
    BOOST_DYNAMIC_BITSET_CONSTEXPR20 void init_from_block_range( BlockIter first, BlockIter last );

    template< typename CharT, typename Traits = std::char_traits< CharT > >
    BOOST_DYNAMIC_BITSET_CONSTEXPR20 void init_from_string( const CharT * s, std::size_t string_length, std::size_t pos, std::size_t n, size_type num_bits );

    BOOST_DYNAMIC_BITSET_CONSTEXPR20 void init_from_unsigned_long( size_type num_bits, unsigned long value /*,
                                                       const allocator_type& alloc*/
    );

    template< typename BlockInputIterator >
    BOOST_DYNAMIC_BITSET_CONSTEXPR20 void m_append( BlockInputIterator first, BlockInputIterator last, std::input_iterator_tag );

    template< typename BlockInputIterator >
    BOOST_DYNAMIC_BITSET_CONSTEXPR20 void             m_append( BlockInputIterator first, BlockInputIterator last, std::forward_iterator_tag );

    BOOST_DYNAMIC_BITSET_CONSTEXPR20 bool             m_unchecked_test( size_type pos ) const;
    static BOOST_DYNAMIC_BITSET_CONSTEXPR20 size_type calc_num_blocks( size_type num_bits );

    BOOST_DYNAMIC_BITSET_CONSTEXPR20 Block &          m_highest_block();
    BOOST_DYNAMIC_BITSET_CONSTEXPR20 const Block &    m_highest_block() const;

    buffer_type                                       m_bits;
    size_type                                         m_num_bits;

    class bit_appender;
    friend class bit_appender;
    class bit_appender
    {
        // Helper for stream >>.
        //
        // Makes up for the lack of an efficient append at the least
        // significant end: bits are actually appended "at left" but
        // rearranged in the destructor.
        //
        dynamic_bitset & bs;
        size_type        n;
        Block            mask;
        Block *          current;

    public:
        bit_appender( const bit_appender & )             = delete;
        bit_appender & operator=( const bit_appender & ) = delete;

        bit_appender( dynamic_bitset & r );
        ~bit_appender();
        void      do_append( bool value );
        size_type get_count() const;
    };
};

template< typename Iterator >
class bit_iterator_base
{
public:
    typedef typename std::iterator_traits< Iterator >::iterator_category iterator_category;
    typedef bool                                 value_type;
    typedef std::ptrdiff_t                       difference_type;
    typedef value_type *                         pointer;
    typedef value_type &                         reference;

    static constexpr int                         bits_per_block = std::numeric_limits< typename std::iterator_traits< Iterator >::value_type >::digits;

    BOOST_DYNAMIC_BITSET_CONSTEXPR20             bit_iterator_base( Iterator block_iterator, int bit_index );

    template< typename Iter >
    friend BOOST_DYNAMIC_BITSET_CONSTEXPR20 bool operator==( const bit_iterator_base< Iter > & lhs, const bit_iterator_base< Iter > & rhs );
    template< typename Iter >
    friend BOOST_DYNAMIC_BITSET_CONSTEXPR20 bool operator<( const bit_iterator_base< Iter > & lhs, const bit_iterator_base< Iter > & rhs );
    template< typename Iter >
    friend BOOST_DYNAMIC_BITSET_CONSTEXPR20 difference_type operator-( const bit_iterator_base< Iter > & lhs, const bit_iterator_base< Iter > & rhs );

protected:
    BOOST_DYNAMIC_BITSET_CONSTEXPR20 void increment();
    BOOST_DYNAMIC_BITSET_CONSTEXPR20 void decrement();
    BOOST_DYNAMIC_BITSET_CONSTEXPR20 void add( typename std::iterator_traits< Iterator >::difference_type n );

    Iterator                              m_block_iterator;
    int                                   m_bit_index = 0;
};

template< typename DynamicBitset >
class bit_iterator
    : public bit_iterator_base< typename DynamicBitset::buffer_type::iterator >
{
public:
    typedef typename DynamicBitset::reference                                                            reference;
    typedef reference *                                                                                  pointer;
    typedef typename bit_iterator_base< typename DynamicBitset::buffer_type::iterator >::difference_type difference_type;

    BOOST_DYNAMIC_BITSET_CONSTEXPR20                                                                     bit_iterator();
    BOOST_DYNAMIC_BITSET_CONSTEXPR20                                                                     bit_iterator( typename DynamicBitset::buffer_type::iterator block_iterator, int bit_index );

    BOOST_DYNAMIC_BITSET_CONSTEXPR20 reference                                                           operator*() const;
    BOOST_DYNAMIC_BITSET_CONSTEXPR20 bit_iterator &                                                      operator++();
    BOOST_DYNAMIC_BITSET_CONSTEXPR20 bit_iterator                                                        operator++( int );
    BOOST_DYNAMIC_BITSET_CONSTEXPR20 bit_iterator &                                                      operator--();
    BOOST_DYNAMIC_BITSET_CONSTEXPR20 bit_iterator                                                        operator--( int );
    BOOST_DYNAMIC_BITSET_CONSTEXPR20 bit_iterator &                                                      operator+=( difference_type n );
    BOOST_DYNAMIC_BITSET_CONSTEXPR20 bit_iterator &                                                      operator-=( difference_type n );
    BOOST_DYNAMIC_BITSET_CONSTEXPR20 reference                                                           operator[]( difference_type n ) const;
};

template< typename DynamicBitset >
class const_bit_iterator
    : public bit_iterator_base< typename DynamicBitset::buffer_type::const_iterator >
{
public:
    typedef bool                                                                                               reference;
    typedef bool                                                                                               const_reference;
    typedef const bool *                                                                                       pointer;
    typedef typename bit_iterator_base< typename DynamicBitset::buffer_type::const_iterator >::difference_type difference_type;

    BOOST_DYNAMIC_BITSET_CONSTEXPR20                                                                           const_bit_iterator( typename DynamicBitset::buffer_type::const_iterator block_iterator, int bit_index );
    BOOST_DYNAMIC_BITSET_CONSTEXPR20                                                                           const_bit_iterator( const bit_iterator< DynamicBitset > & it );

    BOOST_DYNAMIC_BITSET_CONSTEXPR20 const_reference                                                           operator*() const;
    BOOST_DYNAMIC_BITSET_CONSTEXPR20 const_bit_iterator &                                                      operator++();
    BOOST_DYNAMIC_BITSET_CONSTEXPR20 const_bit_iterator                                                        operator++( int );
    BOOST_DYNAMIC_BITSET_CONSTEXPR20 const_bit_iterator &                                                      operator--();
    BOOST_DYNAMIC_BITSET_CONSTEXPR20 const_bit_iterator                                                        operator--( int );
    BOOST_DYNAMIC_BITSET_CONSTEXPR20 const_bit_iterator &                                                      operator+=( difference_type n );
    BOOST_DYNAMIC_BITSET_CONSTEXPR20 const_bit_iterator &                                                      operator-=( difference_type n );
    BOOST_DYNAMIC_BITSET_CONSTEXPR20 const_reference                                                           operator[]( difference_type n ) const;
};

//!     Compares two bitsets.
//!
//!     \return
//!     `true` if `a.size() == b.size()` and for all `i` in the range
//!     `[0, a.size())`, `a[ i ] == b[ i ]`. Otherwise `false`.
//!
//!     \par Throws
//!     Nothing.
//!
//!     (Required by <a href="https://en.cppreference.com/w/cpp/named_req/EqualityComparable">EqualityComparable</a>.)
// -----------------------------------------------------------------------
template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 bool operator==( const dynamic_bitset< Block, AllocatorOrContainer > & a, const dynamic_bitset< Block, AllocatorOrContainer > & b );

//!     Compares two bitsets.
//!
//!     \return
//!     `! ( a == b )`.
//!
//!     \par Throws
//!     Nothing.
// -----------------------------------------------------------------------
template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 bool operator!=( const dynamic_bitset< Block, AllocatorOrContainer > & a, const dynamic_bitset< Block, AllocatorOrContainer > & b );

//!     Compares two bitsets.
//!
//!     \return
//!     `true` if `a` is lexicographically less than `b`, otherwise
//!     `false`.
//!
//!     \par Throws
//!     Nothing.
//!
//!     (Required by <a href="https://en.cppreference.com/w/cpp/named_req/LessThanComparable">LessThanComparable</a>.)
// -----------------------------------------------------------------------
template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 bool operator<( const dynamic_bitset< Block, AllocatorOrContainer > & a, const dynamic_bitset< Block, AllocatorOrContainer > & b );

//!     Compares two bitsets.
//!
//!     \return
//!     `a < b || a == b`.
//!
//!     \par Throws
//!     Nothing.
// -----------------------------------------------------------------------
template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 bool operator<=( const dynamic_bitset< Block, AllocatorOrContainer > & a, const dynamic_bitset< Block, AllocatorOrContainer > & b );

//!     Compares two bitsets.
//!
//!     \return
//!     `b < a`.
//!
//!     \par Throws
//!     Nothing.
// -----------------------------------------------------------------------
template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 bool operator>( const dynamic_bitset< Block, AllocatorOrContainer > & a, const dynamic_bitset< Block, AllocatorOrContainer > & b );

//!     Compares two bitsets.
//!
//!     \return
//!     `b <= a`.
//!
//!     \par Throws
//!     Nothing.
// -----------------------------------------------------------------------
template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 bool operator>=( const dynamic_bitset< Block, AllocatorOrContainer > & a, const dynamic_bitset< Block, AllocatorOrContainer > & b );

//!     Inserts a textual representation of `b` into the stream `os`,
//!     highest bit first.
//!
//!     Informally, the output is the same as:
//!
//!     \code
//!     std::basic_string<Char, Traits> s;
//!     boost::to_string(x, s):
//!     os << s;
//!     \endcode
//!
//!     except that the stream inserter takes into accout the locale
//!     imbued into `os`, which `boost::to_string()` can't do. More
//!     precisely: First, for each valid position `i` into the bitset b
//!     let's put: `character_of( b[ i ) ] ) = b[ i ] ? os.widen( '1' )
//!     : os.widen( '0' );`. Let also `s` be a `std::basic_string<Char,
//!     Traits>` object, having length `b.size()` and such that, for
//!     each `i` in `[0, b.size())`, `s[ i ]` is `character_of( b[ i ]
//!     )`. Then, the output, the effects on `os` and the exception
//!     behavior is the same as outputting the object `s` to `os` (same
//!     width, same exception mask, same padding, same `setstate()`
//!     logic.)
//!
//!     \return
//!     `os`.
// -----------------------------------------------------------------------
template< typename CharT, typename Traits, typename Block, typename AllocatorOrContainer >
std::basic_ostream< CharT, Traits > &
operator<<( std::basic_ostream< CharT, Traits > & os, const dynamic_bitset< Block, AllocatorOrContainer > & b );

//!     Extracts a `dynamic_bitset` from an input stream.
//!
//!     \par Definitions
//!     - A (non-eof) character `c` extracted from `is` is a <em>bitset
//!     digit</em> if and only if either `Traits::eq(c, is.widen('0'))`
//!     or `Traits::eq(c, is.widen('1'))` return `true`.
//!
//!     - If `c` is a bitset digit, its corresponding bit value is 0 if
//!     `Tr::eq(c, is.widen('0'))` returns true, 1 otherwise.
//!
//!     The extractor begins by constructing a `sentry` object `k` as if
//!     by `typename std::basic_istream< Char, Traits >::sentry k( is
//!     )`. If `bool( k )` is `true`, it calls `b.clear()` then attempts
//!     to extract characters from `is`. For each character `c` that is
//!     a bitset digit, the corresponding bit value is appended to the
//!     less significant end of `b` (appending may throw). If
//!     `is.width()` is greater than zero and smaller than
//!     `b.max_size()` then the maximum number `n` of bits appended is
//!     `is.width()`; otherwise `n = b.max_size()`. Unless the extractor
//!     is exited via an exception, characters are extracted (and
//!     corresponding bits appended) until any of the following occurs:
//!
//!     - `n` bits are stored into the bitset;
//!     - end-of-file, or an error, occurs on the input sequence;
//!     - the next available input character isn't a bitset digit.
//!
//!     If no exception caused the function to exit then `is.width( 0 )`
//!     is called, regardless of how many characters were actually
//!     extracted. The sentry object `k` is destroyed.
//!
//!     If the function extracts no characters, it calls `is.setstate(
//!     std::ios::failbit )`, which may throw `std::ios_base::failure`.
//!
//!     \return
//!     `is`.
// -----------------------------------------------------------------------
template< typename CharT, typename Traits, typename Block, typename AllocatorOrContainer >
std::basic_istream< CharT, Traits > &
operator>>( std::basic_istream< CharT, Traits > & is, dynamic_bitset< Block, AllocatorOrContainer > & b );

//!     Performs a bitwise-AND of two bitsets.
//!
//!     \pre
//!     `a.size() == b.size()`.
//!
//!     \return
//!     A new bitset which is the bitwise-AND of the bitsets `a` and
//!     `b`.
//!
//!     \par Throws
//!     An allocation error if memory is exhausted (`std::bad_alloc` if
//!     `AllocatorOrContainer` is a `std::allocator`).
// -----------------------------------------------------------------------
template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 dynamic_bitset< Block, AllocatorOrContainer >
                                 operator&( const dynamic_bitset< Block, AllocatorOrContainer > & a, const dynamic_bitset< Block, AllocatorOrContainer > & b );

//!     Performs a bitwise-OR of two bitsets.
//!
//!     \pre
//!     `a.size() == b.size()`.
//!
//!     \return
//!     A new bitset which is the bitwise-OR of the bitsets `a` and `b`.
//!
//!     \par Throws
//!     An allocation error if memory is exhausted (`std::bad_alloc` if
//!     `allocator_type` is a `std::allocator`).
// -----------------------------------------------------------------------
template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 dynamic_bitset< Block, AllocatorOrContainer >
                                 operator|( const dynamic_bitset< Block, AllocatorOrContainer > & a, const dynamic_bitset< Block, AllocatorOrContainer > & b );

//!     Performs a bitwise-XOR of two bitsets.
//!
//!     \pre
//!     `a.size() == b.size()`.
//!
//!     \return
//!     A new bitset which is the bitwise-XOR of the bitsets `a` and
//!     `b`.
//!
//!     \par Throws
//!     An allocation error if memory is exhausted (`std::bad_alloc` if
//!     `allocator_type` is a `std::allocator`).
// -----------------------------------------------------------------------
template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 dynamic_bitset< Block, AllocatorOrContainer >
                                 operator^( const dynamic_bitset< Block, AllocatorOrContainer > & a, const dynamic_bitset< Block, AllocatorOrContainer > & b );

//!     Calculates the set difference of two bitsets.
//!
//!     \pre
//!     `a.size() == b.size()`.
//!
//!     \return
//!     A new bitset which is the set difference of the bitsets `a` and
//!     `b`.
//!
//!     \par Throws
//!     An allocation error if memory is exhausted (`std::bad_alloc` if
//!     `allocator_type` is a `std::allocator`).
// -----------------------------------------------------------------------
template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 dynamic_bitset< Block, AllocatorOrContainer >
                                 operator-( const dynamic_bitset< Block, AllocatorOrContainer > & a, const dynamic_bitset< Block, AllocatorOrContainer > & b );

//!     Exchanges the contents of `a` and `b`.
//!
//!     \param a The bitset to exchange the contents of with `b`.
//!     \param b The bitset to exchange the contents of with `a`.
//!
//!     \par Throws
//!     Nothing.
// -----------------------------------------------------------------------
template< typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 void swap( dynamic_bitset< Block, AllocatorOrContainer > & a, dynamic_bitset< Block, AllocatorOrContainer > & b ) noexcept;

//!     Copies a representation of `b` into the string `s`.
//!
//!     Character position `i` in the string corresponds to bit position
//!     `b.size() - 1 - i`.
//!
//!     \par Throws
//!     An allocation error from `s` if memory is exhausted.
//!
//!     \par Rationale
//!     This function is not a member function taking zero arguments and
//!     returning a string for a couple of historical reasons. First, this
//!     version could be slightly more efficient because the string is not
//!     copied. Second, as a member function, to allow for flexibility with
//!     regards to the template parameters of `basic_string`, the member
//!     function would require explicit template parameters. Few C++ programmers
//!     were familiar with explicit template parameters, and some C++ compilers
//!     did not handle them properly.
//!
//!     \param b The bitset of which to copy the representation.
//!     \param s The string in which to copy the representation.
// -----------------------------------------------------------------------
template< typename Block, typename AllocatorOrContainer, typename StringT >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 void
to_string( const dynamic_bitset< Block, AllocatorOrContainer > & b, StringT & s );

//!     Writes the bits of the bitset into the iterator `result`, a
//!     block at a time.
//!
//!     The first block written represents the bits in the position
//!     range `[0, bits_per_block)` in the bitset, the second block
//!     written the bits in the range `[bits_per_block, 2 \* bits_per_block)`,
//!     and so on. For each block `bval` written, the bit
//!     `( bval >> i ) & 1` corresponds to the bit at position
//!     `b \* bits_per_block + i` in the bitset.
//!
//!     \pre
//!     The type `BlockOutputIterator` must be a model of
//!     <a href="https://en.cppreference.com/w/cpp/named_req/OutputIterator">LegacyOutputIterator</a>
//!     and its `value_type` must be the same type as `Block`.
//!     Furthermore, the size of the output range must be greater than
//!     or equal to `b.num_blocks()`.
//!
//!     \param b The bitset of which to copy the bits.
//!     \param result The start of the range to write to.
// -----------------------------------------------------------------------
template< typename Block, typename AllocatorOrContainer, typename BlockOutputIterator >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 void
to_block_range( const dynamic_bitset< Block, AllocatorOrContainer > & b, BlockOutputIterator result );

//!     Reads blocks from the iterator range into the bitset.
//!
//!     \pre
//!     The type `BlockIterator` must be a model of
//!     <a href="https://en.cppreference.com/w/cpp/named_req/InputIterator">LegacyInputIterator</a>
//!     and its `value_type` must be the same type as `Block`. The size
//!     of the iterator range must be less than or equal to
//!     `b.num_blocks()`. Excess bits are not copied into the bitset,
//!     which doesn't change its size.
//!
//!     \param first The start of the range.
//!     \param last The end of the range.
//!     \param result The resulting bitset.
// -----------------------------------------------------------------------
template< typename BlockIterator, typename Block, typename AllocatorOrContainer >
BOOST_DYNAMIC_BITSET_CONSTEXPR20 void
from_block_range( BlockIterator first, BlockIterator last, dynamic_bitset< Block, AllocatorOrContainer > & result );

}

#include "boost/dynamic_bitset/impl/dynamic_bitset.ipp"
#endif // include guard
