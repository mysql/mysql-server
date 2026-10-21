//  lock-free single-producer/single-consumer ringbuffer
//  this algorithm is implemented in various projects (linux kernel)
//
//  Copyright (C) 2009-2013 Tim Blechmann
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_LOCKFREE_SPSC_QUEUE_HPP_INCLUDED
#define BOOST_LOCKFREE_SPSC_QUEUE_HPP_INCLUDED

#include <boost/config.hpp>
#ifdef BOOST_HAS_PRAGMA_ONCE
#    pragma once
#endif

#include <algorithm>
#include <array>
#include <cstddef>
#include <memory>
#include <type_traits>

#include <boost/assert.hpp>
#include <boost/core/allocator_access.hpp>
#include <boost/core/span.hpp>
#include <boost/parameter/optional.hpp>
#include <boost/parameter/parameters.hpp>

#include <boost/lockfree/detail/atomic.hpp>
#include <boost/lockfree/detail/parameter.hpp>
#include <boost/lockfree/detail/power_of_two.hpp>
#include <boost/lockfree/detail/prefix.hpp>
#include <boost/lockfree/detail/uses_optional.hpp>
#include <boost/lockfree/lockfree_forward.hpp>

namespace boost { namespace lockfree {

/** The spsc_queue class provides a single-writer/single-reader fifo queue, pushing and popping is wait-free.
 *
 *  \b Policies:
 *  - \c boost::lockfree::capacity<>, optional <br>
 *    If this template argument is passed to the options, the size of the ringbuffer is set at compile-time.
 *
 *  - \c boost::lockfree::allocator<>, defaults to \c boost::lockfree::allocator<std::allocator<T>> <br>
 *    Specifies the allocator that is used to allocate the ringbuffer. This option is only valid, if the ringbuffer is
 * configured to be sized at run-time
 *
 *  \b Requirements:
 *  - T must have a default constructor
 *  - T must be copyable or movable
 * */
template < typename T, typename... Options >
#if !defined( BOOST_NO_CXX20_HDR_CONCEPTS )
    requires( std::is_default_constructible_v< T >, std::is_move_assignable_v< T > || std::is_copy_assignable_v< T > )
#endif
class spsc_queue
{
private:
#ifndef BOOST_DOXYGEN_INVOKED
    typedef parameter::parameters< boost::parameter::optional< tag::capacity >, boost::parameter::optional< tag::allocator > >
        spsc_queue_signature;

    typedef typename spsc_queue_signature::bind< Options... >::type bound_args;

    static constexpr bool   has_capacity          = detail::extract_capacity< bound_args >::has_capacity;
    static constexpr size_t compile_time_capacity = detail::extract_capacity< bound_args >::capacity;
    static constexpr bool   runtime_sized         = !has_capacity;

    typedef detail::extract_allocator_t< bound_args, T > allocator_arg;
    static constexpr bool has_allocator_arg = detail::extract_allocator< bound_args, T >::has_allocator;

    static_assert( !( has_capacity && has_allocator_arg ), "spsc_queue: capacity and allocator are mutually exclusive" );

    // ---- storage types ----

    struct compile_time_storage
    {
        static constexpr size_t buffer_capacity_ = compile_time_capacity + 1;
        static constexpr size_t buffer_mask_ = detail::is_power_of_two( buffer_capacity_ ) ? buffer_capacity_ - 1 : 0;

        T* data()
        {
            return reinterpret_cast< T* >( storage_.data() );
        }

        const T* data() const
        {
            return reinterpret_cast< const T* >( storage_.data() );
        }

        std::size_t next_index( std::size_t arg ) const
        {
            if ( buffer_mask_ )
                return ( arg + 1 ) & buffer_mask_;
            std::size_t ret = arg + 1;
            while ( BOOST_UNLIKELY( ret >= buffer_capacity_ ) )
                ret -= buffer_capacity_;
            return ret;
        }

        alignas( T ) std::array< unsigned char, buffer_capacity_ * sizeof( T ) > storage_;
    };

    struct runtime_storage : private allocator_arg
    {
        runtime_storage() = default;

        runtime_storage( size_t max_elements, allocator_arg const& alloc = {} ) :
            allocator_arg( alloc ),
            buffer_capacity_( max_elements + 1 ),
            buffer_( allocator_arg::allocate( buffer_capacity_ ) )
        {}

        ~runtime_storage()
        {
            if ( buffer_ )
                allocator_arg::deallocate( buffer_, buffer_capacity_ );
        }

        runtime_storage( const runtime_storage& )            = delete;
        runtime_storage& operator=( const runtime_storage& ) = delete;

        T* data()
        {
            return buffer_;
        }

        const T* data() const
        {
            return buffer_;
        }

        std::size_t next_index( std::size_t arg ) const
        {
            std::size_t ret = arg + 1;
            while ( BOOST_UNLIKELY( ret >= buffer_capacity_ ) )
                ret -= buffer_capacity_;
            return ret;
        }

        const size_t buffer_capacity_ = 0;
        T* const     buffer_          = nullptr;
    };

    typedef std::conditional_t< has_capacity, compile_time_storage, runtime_storage > storage_type;

    // ---- helpers ----

    static bool empty( size_t write_index, size_t read_index )
    {
        return write_index == read_index;
    }

    static size_t read_available( size_t write_index, size_t read_index, size_t max_size )
    {
        if ( write_index >= read_index )
            return write_index - read_index;

        const size_t ret = write_index + max_size - read_index;
        return ret;
    }

    static size_t write_available( size_t write_index, size_t read_index, size_t max_size )
    {
        size_t ret = read_index - write_index - 1;
        if ( write_index >= read_index )
            ret += max_size;
        return ret;
    }

    template < class OutputIterator >
    static OutputIterator move_and_delete( T* first, T* last, OutputIterator out )
    {
        if ( std::is_trivially_destructible< T >::value ) {
            return std::copy( first, last, out );
        } else {
            for ( ; first != last; ++first, ++out ) {
                *out = std::move( *first );
                first->~T();
            }
            return out;
        }
    }

    template < class Functor >
    static void run_functor_and_delete( T* first, T* last, Functor&& functor )
    {
        for ( ; first != last; ++first ) {
            functor( std::move( *first ) );
            first->~T();
        }
    }

    struct implementation_defined
    {
        typedef allocator_arg allocator;
        typedef std::size_t   size_type;
    };

#endif

public:
    typedef T                                          value_type;
    typedef typename implementation_defined::allocator allocator;
    typedef typename implementation_defined::size_type size_type;

    // ---- constructors ----

    /** Constructs a spsc_queue
     *
     *  \pre spsc_queue must be configured to be sized at compile-time
     */
    spsc_queue()
#if !defined( BOOST_NO_CXX20_HDR_CONCEPTS )
        requires( !runtime_sized )
#endif
    {
        BOOST_ASSERT( !runtime_sized );
    }

    /** Constructs a spsc_queue with a custom allocator
     *
     *  \pre spsc_queue must be configured to be sized at compile-time
     *
     *  \note This is just for API compatibility: an allocator isn't actually needed
     */
#if !defined( BOOST_NO_CXX20_HDR_CONCEPTS )
    template < typename U >
        requires( !runtime_sized )
#else
    template < typename U, typename Enabler = std::enable_if< !runtime_sized > >
#endif
    explicit spsc_queue( typename boost::allocator_rebind< allocator, U >::type const& )
    {}

    /** Constructs a spsc_queue with a custom allocator
     *
     *  \pre spsc_queue must be configured to be sized at compile-time
     *
     *  \note This is just for API compatibility: an allocator isn't actually needed
     */
#if !defined( BOOST_NO_CXX20_HDR_CONCEPTS )
    explicit spsc_queue( allocator const& )
        requires( !runtime_sized )
#else
    template < typename Enabler = std::enable_if< !runtime_sized > >
    explicit spsc_queue( allocator const& )
#endif
    {}

    /** Constructs a spsc_queue for element_count elements
     *
     *  \pre spsc_queue must be configured to be sized at run-time
     */
#if !defined( BOOST_NO_CXX20_HDR_CONCEPTS )
    explicit spsc_queue( size_type element_count )
        requires( runtime_sized )
#else
    template < typename Enabler = std::enable_if< runtime_sized > >
    explicit spsc_queue( size_type element_count )
#endif
        :
        storage_( element_count )
    {}

    /** Constructs a spsc_queue for element_count elements with a custom allocator
     *
     *  \pre spsc_queue must be configured to be sized at run-time
     */
#if !defined( BOOST_NO_CXX20_HDR_CONCEPTS )
    template < typename U >
        requires( runtime_sized )
#else
    template < typename U, typename Enabler = std::enable_if< runtime_sized > >
#endif
    spsc_queue( size_type element_count, typename boost::allocator_rebind< allocator, U >::type const& alloc ) :
        storage_( element_count, alloc )
    {}

    /** Constructs a spsc_queue for element_count elements with a custom allocator
     *
     *  \pre spsc_queue must be configured to be sized at run-time
     */
#if !defined( BOOST_NO_CXX20_HDR_CONCEPTS )
    spsc_queue( size_type element_count, allocator const& alloc )
        requires( runtime_sized )
#else
    template < typename Enabler = std::enable_if< runtime_sized > >
    spsc_queue( size_type element_count, allocator const& alloc )
#endif
        :
        storage_( element_count, alloc )
    {}

    spsc_queue( const spsc_queue& )            = delete;
    spsc_queue& operator=( const spsc_queue& ) = delete;
    spsc_queue( spsc_queue&& )                 = delete;
    spsc_queue& operator=( spsc_queue&& )      = delete;

    /** Destroys the spsc_queue, calling destructors on all remaining elements.
     */
    ~spsc_queue()
    {
        consume_all( []( const T& ) {} );
    }

    // ---- reset ----

    /** reset the ringbuffer
     *
     * \note Not thread-safe
     * */
    void reset()
    {
        if ( !std::is_trivially_destructible< T >::value ) {
            consume_all( []( const T& ) {} );
        } else {
            write_index_.store( 0, memory_order_relaxed );
            read_index_.store( 0, memory_order_release );
        }
    }

    // ---- capacity / empty ----

    /** Check if the ringbuffer is empty
     *
     * \return true, if the ringbuffer is empty, false otherwise
     * \note Due to the concurrent nature of the ringbuffer the result may be inaccurate.
     * */
    bool empty()
    {
        return empty( write_index_.load( memory_order_relaxed ), read_index_.load( memory_order_relaxed ) );
    }

    /**
     * \return true, if implementation is lock-free.
     *
     * */
    bool is_lock_free() const
    {
        return write_index_.is_lock_free() && read_index_.is_lock_free();
    }

    // ---- single-element push ----

    /** Pushes value t to the ringbuffer.
     *
     * \pre only one thread is allowed to push data to the spsc_queue
     * \post object will be pushed to the spsc_queue, unless it is full.
     * \return true, if the push operation is successful.
     *
     * \note Thread-safe and wait-free
     * */
    bool push( const T& t )
    {
        const size_t write_index = write_index_.load( memory_order_relaxed );
        const size_t next        = storage_.next_index( write_index );

        if ( next == read_index_.load( memory_order_acquire ) )
            return false;

        new ( storage_.data() + write_index ) T( t );

        write_index_.store( next, memory_order_release );

        return true;
    }

    /** Pushes value t to the ringbuffer.
     *
     * \pre only one thread is allowed to push data to the spsc_queue
     * \post object will be pushed to the spsc_queue, unless it is full.
     * \return true, if the push operation is successful.
     *
     * \note Thread-safe and wait-free
     * */
    bool push( T&& t )
    {
        const size_t write_index = write_index_.load( memory_order_relaxed );
        const size_t next        = storage_.next_index( write_index );

        if ( next == read_index_.load( memory_order_acquire ) )
            return false;

        new ( storage_.data() + write_index ) T( std::forward< T >( t ) );

        write_index_.store( next, memory_order_release );

        return true;
    }

    // ---- range push ----

    /** Pushes as many objects from the input range as there is space.
     *
     * \pre only one thread is allowed to push data to the spsc_queue
     * \return number of pushed items
     *
     * \note Thread-safe and wait-free
     * */
    size_t push( const T* input_buffer, size_t input_count )
    {
        return push( input_buffer, input_buffer + input_count ) - input_buffer;
    }

    /** Pushes as many objects from the range [begin, end) as there is space.
     *
     * \pre only one thread is allowed to push data to the spsc_queue
     * \return iterator to the first element, which has not been pushed
     *
     * \note Thread-safe and wait-free
     * */
    template < typename ConstIterator >
    ConstIterator push( ConstIterator begin, ConstIterator end )
    {
        const size_t write_index = write_index_.load( memory_order_relaxed );
        const size_t read_index  = read_index_.load( memory_order_acquire );
        const size_t max_size    = storage_.buffer_capacity_;
        const size_t avail       = write_available( write_index, read_index, max_size );

        if ( avail == 0 )
            return begin;

        size_t input_count = std::distance( begin, end );
        input_count        = (std::min)( input_count, avail );

        size_t new_write_index = write_index + input_count;

        const ConstIterator last = std::next( begin, input_count );

        T* buffer = storage_.data();

        if ( write_index + input_count > max_size ) {
            const size_t        count0   = max_size - write_index;
            const ConstIterator midpoint = std::next( begin, count0 );

            std::uninitialized_copy( begin, midpoint, buffer + write_index );
            std::uninitialized_copy( midpoint, last, buffer );
            new_write_index -= max_size;
        } else {
            std::uninitialized_copy( begin, last, buffer + write_index );

            if ( new_write_index == max_size )
                new_write_index = 0;
        }

        write_index_.store( new_write_index, memory_order_release );
        return last;
    }

    /** Pushes as many objects from the array t as there is space available.
     *
     * \pre only one thread is allowed to push data to the spsc_queue
     * \return number of pushed items
     *
     * \note Thread-safe and wait-free
     */
    template < size_type size >
    size_type push( T const ( &t )[ size ] )
    {
        return this->push( t, size );
    }

    /** Pushes as many objects from the span t as there is space available.
     *
     * \pre only one thread is allowed to push data to the spsc_queue
     * \return number of pushed items
     *
     * \note Thread-safe and wait-free
     */
    template < std::size_t Extent >
    size_type push( boost::span< const T, Extent > t )
    {
        return this->push( t.data(), t.size() );
    }

    // ---- single-element consume ----

    /** Consumes one element via a functor.
     *
     *  Pops one element from the queue and applies the functor on this object.
     *
     * \returns true, if one element was consumed
     *
     * \note Thread-safe and non-blocking, if functor is thread-safe and non-blocking
     * */
    template < typename Functor >
    bool consume_one( Functor&& functor )
    {
        const size_t write_index = write_index_.load( memory_order_acquire );
        const size_t read_index  = read_index_.load( memory_order_relaxed );
        if ( empty( write_index, read_index ) )
            return false;

        T* buffer            = storage_.data();
        T& object_to_consume = buffer[ read_index ];
        functor( std::move( object_to_consume ) );
        object_to_consume.~T();

        size_t next = storage_.next_index( read_index );
        read_index_.store( next, memory_order_release );
        return true;
    }

    /** Pops one object from ringbuffer.
     *
     * \pre only one thread is allowed to pop data from the spsc_queue
     * \post if ringbuffer is not empty, object will be discarded.
     * \return true, if the pop operation is successful, false if ringbuffer was empty.
     *
     * \note Thread-safe and wait-free
     * */
    bool pop()
    {
        return consume_one( []( const T& ) {} );
    }

    // ---- typed pop ----

    /** Pops one object from ringbuffer.
     *
     * \pre only one thread is allowed to pop data from the spsc_queue
     * \post if ringbuffer is not empty, object will be copied to ret.
     * \return true, if the pop operation is successful, false if ringbuffer was empty.
     *
     * \note Thread-safe and wait-free
     */
#if !defined( BOOST_NO_CXX20_HDR_CONCEPTS )
    template < typename U >
        requires( std::is_convertible_v< T, U > )
#else
    template < typename U, typename Enabler = std::enable_if_t< std::is_convertible< T, U >::value > >
#endif
    bool pop( U& ret )
    {
        return this->consume_one( [ & ]( T&& t ) {
            ret = std::forward< T >( t );
        } );
    }

#if !defined( BOOST_NO_CXX17_HDR_OPTIONAL ) || defined( BOOST_DOXYGEN_INVOKED )
    /** Pops object from spsc_queue, returning a std::optional<>
     *
     * \returns `std::optional` with value if successful, `std::nullopt` if spsc_queue is empty.
     *
     * \note Thread-safe and non-blocking
     *
     * */
    std::optional< T > pop( uses_optional_t )
    {
        T to_dequeue;
        if ( pop( to_dequeue ) )
            return to_dequeue;
        else
            return std::nullopt;
    }

    /** Pops object from spsc_queue, returning a std::optional<>
     *
     * \pre type T must be convertible to U
     * \returns `std::optional` with value if successful, `std::nullopt` if spsc_queue is empty.
     *
     * \note Thread-safe and non-blocking
     *
     * */
    template < typename U >
    std::optional< U > pop( uses_optional_t )
    {
        U to_dequeue;
        if ( pop( to_dequeue ) )
            return to_dequeue;
        else
            return std::nullopt;
    }
#endif

    // ---- batch consume ----

    /** Consumes all elements via a functor.
     *
     *  Sequentially pops all elements from the queue and applies the functor on each object.
     *
     * \returns number of elements that are consumed
     *
     * \note Thread-safe and non-blocking, if functor is thread-safe and non-blocking
     * */
    template < typename Functor >
    size_t consume_all( Functor&& functor )
    {
        const size_t write_index = write_index_.load( memory_order_acquire );
        const size_t read_index  = read_index_.load( memory_order_relaxed );
        const size_t max_size    = storage_.buffer_capacity_;

        const size_t avail = read_available( write_index, read_index, max_size );

        if ( avail == 0 )
            return 0;

        const size_t output_count = avail;

        size_t new_read_index = read_index + output_count;

        T* buffer = storage_.data();

        if ( read_index + output_count > max_size ) {
            const size_t count0 = max_size - read_index;
            const size_t count1 = output_count - count0;

            run_functor_and_delete( buffer + read_index, buffer + max_size, functor );
            run_functor_and_delete( buffer, buffer + count1, functor );

            new_read_index -= max_size;
        } else {
            run_functor_and_delete( buffer + read_index, buffer + read_index + output_count, functor );

            if ( new_read_index == max_size )
                new_read_index = 0;
        }

        read_index_.store( new_read_index, memory_order_release );
        return output_count;
    }

    // ---- range pop ----

    /** Pops a maximum of count objects from ringbuffer.
     *
     * \pre only one thread is allowed to pop data from the spsc_queue
     * \return number of popped items
     *
     * \note Thread-safe and wait-free
     * */
    size_t pop( T* output_buffer, size_t output_count )
    {
        const size_t write_index = write_index_.load( memory_order_acquire );
        const size_t read_index  = read_index_.load( memory_order_relaxed );
        const size_t max_size    = storage_.buffer_capacity_;

        const size_t avail = read_available( write_index, read_index, max_size );

        if ( avail == 0 )
            return 0;

        output_count = (std::min)( output_count, avail );

        size_t new_read_index = read_index + output_count;

        T* buffer = storage_.data();

        if ( read_index + output_count > max_size ) {
            const size_t count0 = max_size - read_index;
            const size_t count1 = output_count - count0;

            move_and_delete( buffer + read_index, buffer + max_size, output_buffer );
            move_and_delete( buffer, buffer + count1, output_buffer + count0 );

            new_read_index -= max_size;
        } else {
            move_and_delete( buffer + read_index, buffer + read_index + output_count, output_buffer );
            if ( new_read_index == max_size )
                new_read_index = 0;
        }

        read_index_.store( new_read_index, memory_order_release );
        return output_count;
    }

    /** Pops a maximum of size objects from spsc_queue.
     *
     * \pre only one thread is allowed to pop data from the spsc_queue
     * \return number of popped items
     *
     * \note Thread-safe and wait-free
     * */
    template < size_type size >
    size_type pop( T ( &ret )[ size ] )
    {
        return this->pop( ret, size );
    }

    /** Pops objects to the output iterator it.
     *
     * \pre only one thread is allowed to pop data from the spsc_queue
     * \return number of popped items
     *
     * \note Thread-safe and wait-free
     * */
    template < typename OutputIterator >
    size_t pop_to_output_iterator( OutputIterator it )
    {
        const size_t write_index = write_index_.load( memory_order_acquire );
        const size_t read_index  = read_index_.load( memory_order_relaxed );
        const size_t max_size    = storage_.buffer_capacity_;

        const size_t avail = read_available( write_index, read_index, max_size );
        if ( avail == 0 )
            return 0;

        size_t new_read_index = read_index + avail;

        T* buffer = storage_.data();

        if ( read_index + avail > max_size ) {
            const size_t count0 = max_size - read_index;
            const size_t count1 = avail - count0;

            it = move_and_delete( buffer + read_index, buffer + max_size, it );
            move_and_delete( buffer, buffer + count1, it );

            new_read_index -= max_size;
        } else {
            move_and_delete( buffer + read_index, buffer + read_index + avail, it );
            if ( new_read_index == max_size )
                new_read_index = 0;
        }

        read_index_.store( new_read_index, memory_order_release );
        return avail;
    }

    /** Pops objects to the output iterator it.
     *
     * \pre only one thread is allowed to pop data from the spsc_queue
     * \return number of popped items
     *
     * \note Thread-safe and wait-free
     * */
#if !defined( BOOST_NO_CXX20_HDR_CONCEPTS )
    template < typename OutputIterator >
        requires( !std::is_convertible_v< T, OutputIterator > )
    size_type
#else
    template < typename OutputIterator,
               typename Enabler = std::enable_if< !std::is_convertible< T, OutputIterator >::value > >
    typename std::enable_if< !std::is_convertible< T, OutputIterator >::value, size_type >::type
#endif
    pop( OutputIterator it )
    {
        return this->pop_to_output_iterator( it );
    }

    // ---- front ----

    /** get reference to element in the front of the queue
     *
     * \pre only a consuming thread is allowed to check front element
     * \pre read_available() > 0. If ringbuffer is empty, it's undefined behaviour to invoke this method.
     * \return reference to the first element in the queue
     *
     * \note Thread-safe and wait-free
     * */
    const T& front() const
    {
        const size_t read_index = read_index_.load( memory_order_relaxed );
        return *( storage_.data() + read_index );
    }

    /// \copydoc front() const
    T& front()
    {
        const size_t read_index = read_index_.load( memory_order_relaxed );
        return *( storage_.data() + read_index );
    }

    // ---- available ----

    /** get number of elements that are available for read
     *
     * \return number of available elements that can be popped from the spsc_queue
     *
     * \note Thread-safe and wait-free, should only be called from the consumer thread
     * */
    size_t read_available() const
    {
        size_t       write_index = write_index_.load( memory_order_acquire );
        const size_t read_index  = read_index_.load( memory_order_relaxed );
        return read_available( write_index, read_index, storage_.buffer_capacity_ );
    }

    /** get write space to write elements
     *
     * \return number of elements that can be pushed to the spsc_queue
     *
     * \note Thread-safe and wait-free, should only be called from the producer thread
     * */
    size_t write_available() const
    {
        size_t       write_index = write_index_.load( memory_order_relaxed );
        const size_t read_index  = read_index_.load( memory_order_acquire );
        return write_available( write_index, read_index, storage_.buffer_capacity_ );
    }

private:
#ifndef BOOST_DOXYGEN_INVOKED
    alignas( detail::cacheline_bytes ) storage_type storage_;
    alignas( detail::cacheline_bytes ) detail::atomic< size_t > write_index_ {};
    alignas( detail::cacheline_bytes ) detail::atomic< size_t > read_index_ {};
#endif
};

}} // namespace boost::lockfree


#endif /* BOOST_LOCKFREE_SPSC_QUEUE_HPP_INCLUDED */
