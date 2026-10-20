//  lock-free single-producer/single-consumer value
//  implemented via a triple buffer
//
//  Copyright (C) 2023-2024 Tim Blechmann
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_LOCKFREE_SPSC_VALUE_HPP_INCLUDED
#define BOOST_LOCKFREE_SPSC_VALUE_HPP_INCLUDED

#include <boost/config.hpp>

#ifdef BOOST_HAS_PRAGMA_ONCE
#    pragma once
#endif

#include <boost/lockfree/detail/atomic.hpp>
#include <boost/lockfree/detail/parameter.hpp>
#include <boost/lockfree/detail/uses_optional.hpp>
#include <boost/lockfree/lockfree_forward.hpp>
#include <boost/lockfree/policies.hpp>

#include <boost/parameter/optional.hpp>
#include <boost/parameter/parameters.hpp>

#include <array>
#include <cstdint>

#ifndef BOOST_DOXYGEN_INVOKED

#    ifdef BOOST_NO_CXX17_IF_CONSTEXPR
#        define ifconstexpr
#    else
#        define ifconstexpr constexpr
#    endif

#endif

namespace boost { namespace lockfree {

/** The spcs_value provides a single-writer/single-reader value, implemented by a triple buffer
 *
 *  \b Policies:
 *  - \ref boost::lockfree::allow_multiple_reads, defaults to
 *    \ref boost::lockfree::allow_multiple_reads "boost::lockfree::allow_multiple_reads<false>" \n
 *    If multiple reads are allowed, a value written to the spsc_value can be read multiple times, but not moved out
 *    of the instance. If multiple reads are not allowed, the class works as single-element queue that overwrites on
 *    write
 *
 * */
template < typename T, typename... Options >
struct spsc_value
{
#ifndef BOOST_DOXYGEN_INVOKED
private:
    using spsc_value_signature = parameter::parameters< boost::parameter::optional< tag::allow_multiple_reads > >;
    using bound_args           = typename spsc_value_signature::bind< Options... >::type;

    static const bool allow_multiple_reads = detail::extract_allow_multiple_reads< bound_args >::value;

public:
#endif

    /** Construct a \ref boost::lockfree::spsc_value "spsc_value"
     *
     *  If configured with \ref boost::lockfree::allow_multiple_reads "boost::lockfree::allow_multiple_reads<true>" it
     *  is initialized to a default-constructed value
     *
     * */
    explicit spsc_value()
    {
        if ifconstexpr ( allow_multiple_reads ) {
            // populate initial reader
            m_write_index = tagged_index {
                1,
            };
            m_available_index.store(
                tagged_index {
                    0,
                    true,
                },
                std::memory_order_relaxed );
            m_buffer[ 0 ].value = {};
        }
    }

    /** Construct a \ref boost::lockfree::spsc_value "spsc_value", initialized to a value
     * */
    explicit spsc_value( T value ) :
        m_write_index {
            1,
        },
        m_available_index {
            tagged_index {
                0,
                true,
            },
        }
    {
        m_buffer[ 0 ].value = std::move( value );
    }

    /** Writes `value` to the \ref boost::lockfree::spsc_value "spsc_value"
     *
     * \pre  only one thread is allowed to write data to the \ref boost::lockfree::spsc_value "spsc_value"
     * \post object will be written to the \ref boost::lockfree::spsc_value "spsc_value"
     *
     * \note Thread-safe and wait-free
     * */
    void write( T&& value )
    {
        m_buffer[ m_write_index.index() ].value = std::forward< T >( value );
        swap_write_buffer();
    }

    /// \copydoc boost::lockfree::spsc_value::write(T&& value)
    void write( const T& value )
    {
        m_buffer[ m_write_index.index() ].value = value;
        swap_write_buffer();
    }

    /** Reads content of the \ref boost::lockfree::spsc_value "spsc_value"
     *
     * \pre     only one thread is allowed to write data to the \ref boost::lockfree::spsc_value "spsc_value"
     * \post    if read operation is successful, object will be copied to `ret`.
     * \returns `true`, if the read operation is successful, false if the \ref boost::lockfree::spsc_value "spsc_value" is
     *          configured with \ref boost::lockfree::allow_multiple_reads
     *          "boost::lockfree::allow_multiple_reads<false>" and no value is available for reading
     *
     * \note Thread-safe and wait-free
     * */
    bool read( T& ret )
    {
#ifndef BOOST_NO_CXX17_IF_CONSTEXPR
        bool read_index_updated = swap_read_buffer();

        if constexpr ( allow_multiple_reads ) {
            ret = m_buffer[ m_read_index.index() ].value;
        } else {
            if ( !read_index_updated )
                return false;
            ret = std::move( m_buffer[ m_read_index.index() ].value );
        }

        return true;
#else
        return read_helper( ret, std::integral_constant< bool, allow_multiple_reads > {} );
#endif
    }

#if !defined( BOOST_NO_CXX17_HDR_OPTIONAL ) || defined( BOOST_DOXYGEN_INVOKED )
    /** Reads content of the \ref boost::lockfree::spsc_value "spsc_value", returning an optional
     *
     * \pre     only one thread is allowed to write data to the \ref boost::lockfree::spsc_value "spsc_value"
     * \returns `std::optional` with value if successful, `std::nullopt` if spsc_value is configured with \ref
     *          boost::lockfree::allow_multiple_reads "boost::lockfree::allow_multiple_reads<false>" and no value is
     *          available for reading
     *
     * \note Thread-safe and wait-free
     * */
    std::optional< T > read( uses_optional_t )
    {
        T to_dequeue;
        if ( read( to_dequeue ) )
            return to_dequeue;
        else
            return std::nullopt;
    }
#endif

    /** consumes value via a functor
     *
     *  reads element from the spsc_value and applies the functor on this object
     *
     * \returns `true`, if element was consumed
     *
     * \note Thread-safe and non-blocking, if functor is thread-safe and non-blocking
     * */

    template < typename Functor >
    bool consume( Functor&& f )
    {
#ifndef BOOST_NO_CXX17_IF_CONSTEXPR
        bool read_index_updated = swap_read_buffer();

        if constexpr ( allow_multiple_reads ) {
            f( m_buffer[ m_read_index.index() ].value );
        } else {
            if ( !read_index_updated )
                return false;
            f( std::move( m_buffer[ m_read_index.index() ].value ) );
        }

        return true;
#else
        return consume_helper( f, std::integral_constant< bool, allow_multiple_reads > {} );
#endif
    }

private:
#ifndef BOOST_DOXYGEN_INVOKED
    using allow_multiple_reads_true  = std::true_type;
    using allow_multiple_reads_false = std::false_type;

#    ifdef BOOST_NO_CXX17_IF_CONSTEXPR
    template < typename Functor >
    bool consume_helper( Functor&& f, allow_multiple_reads_true = {} )
    {
        swap_read_buffer();
        f( m_buffer[ m_read_index.index() ].value );
        return true;
    }

    template < typename Functor >
    bool consume_helper( Functor&& f, allow_multiple_reads_false = {} )
    {
        bool read_index_updated = swap_read_buffer();
        if ( !read_index_updated )
            return false;
        f( std::move( m_buffer[ m_read_index.index() ].value ) );
        return true;
    }

    template < typename TT >
    bool read_helper( TT& ret, allow_multiple_reads_true = {} )
    {
        swap_read_buffer();
        ret = m_buffer[ m_read_index.index() ].value;
        return true;
    }

    template < typename TT >
    bool read_helper( TT& ret, allow_multiple_reads_false = {} )
    {
        bool read_index_updated = swap_read_buffer();
        if ( !read_index_updated )
            return false;
        ret = std::move( m_buffer[ m_read_index.index() ].value );
        return true;
    }
#    endif

    void swap_write_buffer()
    {
        tagged_index old_avail_index = m_available_index.exchange(
            tagged_index {
                m_write_index.index(),
                true,
            },
            std::memory_order_release );
        m_write_index.set_tag_and_index( old_avail_index.index(), false );
    }

    bool swap_read_buffer()
    {
        constexpr bool use_compare_exchange = false; // exchange is most likely faster

        if ifconstexpr ( use_compare_exchange ) {
            tagged_index new_avail_index = m_read_index;

            tagged_index current_avail_index_with_tag = tagged_index {
                m_available_index.load( std::memory_order_acquire ).index(),
                true,
            };

            if ( m_available_index.compare_exchange_strong( current_avail_index_with_tag,
                                                            new_avail_index,
                                                            std::memory_order_acquire ) ) {
                m_read_index = tagged_index( current_avail_index_with_tag.index(), false );
                return true;
            } else
                return false;
        } else {
            tagged_index new_avail_index = m_read_index;

            tagged_index current_avail_index = m_available_index.load( std::memory_order_acquire );
            if ( !current_avail_index.is_consumable() )
                return false;

            current_avail_index = m_available_index.exchange( new_avail_index, std::memory_order_acquire );
            m_read_index        = tagged_index {
                current_avail_index.index(),
                false,
            };
            return true;
        }
    }

    struct tagged_index
    {
        tagged_index( uint8_t index, bool tag = false )
        {
            set_tag_and_index( index, tag );
        }

        uint8_t index() const
        {
            return byte & 0x07;
        }

        bool is_consumable() const
        {
            return byte & 0x08;
        }

        void set_tag_and_index( uint8_t index, bool tag )
        {
            byte = index | ( tag ? 0x08 : 0x00 );
        }

        uint8_t byte;
    };

    static constexpr size_t cacheline_bytes = detail::cacheline_bytes;

    struct alignas( cacheline_bytes ) cache_aligned_value
    {
        T value;
    };

    std::array< cache_aligned_value, 3 > m_buffer;

    alignas( cacheline_bytes ) tagged_index m_write_index { 0 };
    alignas( cacheline_bytes ) detail::atomic< tagged_index > m_available_index { 1 };
    alignas( cacheline_bytes ) tagged_index m_read_index { 2 };
#endif
};

}} // namespace boost::lockfree

#undef ifconstexpr

#endif /* BOOST_LOCKFREE_SPSC_VALUE_HPP_INCLUDED */
