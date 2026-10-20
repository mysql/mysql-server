//  lock-free bounded ticket queue
//  based on Dmitry Vyukov's bounded MPMC queue algorithm
//  (the "Dyukov turnstile"): each cell carries a monotonically
//  increasing ticket that gates producer and consumer access.
//
//  Copyright (C) 2026 Tim Blechmann
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_LOCKFREE_BOUNDED_TICKET_QUEUE_HPP_INCLUDED
#define BOOST_LOCKFREE_BOUNDED_TICKET_QUEUE_HPP_INCLUDED

#include <boost/config.hpp>
#ifdef BOOST_HAS_PRAGMA_ONCE
#    pragma once
#endif

#include <boost/assert.hpp>
#include <boost/core/allocator_access.hpp>
#include <boost/core/span.hpp>
#include <boost/parameter/optional.hpp>
#include <boost/parameter/parameters.hpp>

#include <boost/lockfree/detail/atomic.hpp>
#include <boost/lockfree/detail/copy_payload.hpp>
#include <boost/lockfree/detail/parameter.hpp>
#include <boost/lockfree/detail/power_of_two.hpp>
#include <boost/lockfree/detail/prefix.hpp>
#include <boost/lockfree/detail/uses_optional.hpp>
#include <boost/lockfree/lockfree_forward.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>

#if defined( _MSC_VER )
#    pragma warning( push )
#    pragma warning( disable : 4324 )
#endif


namespace boost { namespace lockfree {

#ifndef BOOST_DOXYGEN_INVOKED
namespace detail {

typedef parameter::parameters< boost::parameter::optional< tag::allocator >,
                               boost::parameter::optional< tag::capacity >,
                               boost::parameter::optional< tag::single_producer >,
                               boost::parameter::optional< tag::single_consumer > >
    bounded_ticket_queue_signature;

struct multi_producer_tag
{};
struct single_producer_tag
{};
struct multi_consumer_tag
{};
struct single_consumer_tag
{};

} /* namespace detail */
#endif


/** Bounded ticket queue based on Dmitry Vyukov's bounded MPMC
 *  queue (the "Dyukov turnstile")
 *  (<a href="https://www.1024cores.net/home/lock-free-algorithms/queues/bounded-mpmc-queue">reference</a>).
 *
 *  The queue uses a fixed-size ring buffer of cells, each
 *  protected by a monotonically increasing ticket (sequence
 *  number).  Producers and consumers coordinate solely by
 *  advancing this ticket.
 *
 *  The data structure is linearizable.
 *
 *  \par Caveats
 *  - **Multi-producer push is not lock-free.** A concurrent
 *    producer that has claimed a slot via CAS but is then
 *    suspended can block all other producers once the ring
 *    buffer wraps around to that slot.  In practice preemption
 *    is brief, but hard real-time systems should prefer a
 *    single-producer configuration.
 *  - **Multi-consumer pop is not lock-free.** Symmetric
 *    stall risk as multi-producer push.  Prefer
 *    `single_consumer<true>` for hard real-time.
 *  - **Unfairness.** The CAS-based slot reservation provides no
 *    FIFO or fairness guarantee.
 *
 *  \par Policies
 *  - \ref boost::lockfree::capacity (optional):\n
 *    Sets the queue capacity at compile time. Implies a static
 *    storage buffer and precludes specifying an allocator.
 *  - \ref boost::lockfree::allocator (default \c std::allocator<void>).\n
 *    Allocator used for the cell storage. Only valid when the
 *    capacity is specified at run time.
 *  - \ref boost::lockfree::single_producer (default \c false):\n
 *    If true, only one thread calls push.  Push becomes wait-free.
 *  - \ref boost::lockfree::single_consumer (default \c false):\n
 *    If true, only one thread calls pop.  Pop becomes wait-free.
 *
 *  \par Requirements
 *  - T must be copy- or move-constructible, depending on which
 *    \c push overload is used.
 *  - T must be move-constructible and move-assignable.
 */
template < typename T, typename... Options >
class bounded_ticket_queue
{
private:
#ifndef BOOST_DOXYGEN_INVOKED

    typedef typename detail::bounded_ticket_queue_signature::bind< Options... >::type bound_args;

    static constexpr bool   has_capacity          = detail::extract_capacity< bound_args >::has_capacity;
    static constexpr size_t compile_time_capacity = detail::extract_capacity< bound_args >::capacity;

    static constexpr bool is_single_producer = detail::extract_single_producer< bound_args >::value;
    static constexpr bool is_single_consumer = detail::extract_single_consumer< bound_args >::value;

    typedef std::conditional_t< is_single_producer, detail::single_producer_tag, detail::multi_producer_tag > producer_tag;
    typedef std::conditional_t< is_single_consumer, detail::single_consumer_tag, detail::multi_consumer_tag > consumer_tag;

    struct cell
    {
        detail::atomic< size_t > sequence;
        alignas( T ) std::array< unsigned char, sizeof( T ) > storage_buffer;

        template < typename U >
        void construct_from( U&& v, size_t pos )
        {
            ::new ( storage_buffer.data() ) T( std::forward< U >( v ) );
            sequence.store( pos, detail::memory_order_release );
        }

        T take()
        {
            struct destroy_on_exit
            {
                cell& c;
                ~destroy_on_exit()
                {
                    reinterpret_cast< T* >( c.storage_buffer.data() )->~T();
                }
            } guard { *this };

            T* ptr = reinterpret_cast< T* >( storage_buffer.data() );

            return std::move( *ptr );
        }
    };

    // ---- allocator ----

    typedef detail::extract_allocator_t< bound_args, cell > cell_allocator;

    static constexpr bool has_allocator_arg = detail::extract_allocator< bound_args, cell >::has_allocator;

    static_assert( !( has_capacity && has_allocator_arg ),
                   "bounded_ticket_queue: capacity and allocator are mutually exclusive" );

    // ---- storage policies ----

    struct alignas( detail::cacheline_bytes ) compile_time_storage
    {
        compile_time_storage()
        {
            for ( cell& c : buffer_ )
                c.sequence.store( &c - buffer_.data(), detail::memory_order_relaxed );
        }

        static constexpr size_t buffer_capacity_ = compile_time_capacity;
        static constexpr size_t buffer_mask_     = compile_time_capacity - 1;
        static constexpr bool   is_pow2_         = detail::is_power_of_two( compile_time_capacity );

        std::array< cell, compile_time_capacity > buffer_ {};

        cell& get_cell( size_t pos )
        {
            size_t index = is_pow2_ ? ( pos & buffer_mask_ ) : ( pos % buffer_capacity_ );
            return buffer_[ index ];
        }
    };

    struct alignas( detail::cacheline_bytes ) runtime_storage : private cell_allocator
    {
        runtime_storage() = default;

        runtime_storage( size_t n, cell_allocator const& alloc = {} ) :
            cell_allocator( alloc ),
            buffer_capacity_ { n },
            buffer_mask_ { detail::is_power_of_two( n ) ? n - 1 : 0 }
        {
            BOOST_ASSERT( n >= 2 );
            buffer_ = cell_allocator::allocate( n );

            for ( cell* c = buffer_; c != buffer_ + n; ++c )
                c->sequence.store( c - buffer_, detail::memory_order_relaxed );
        }

        ~runtime_storage()
        {
            if ( buffer_ )
                cell_allocator::deallocate( buffer_, buffer_capacity_ );
        }

        cell*        buffer_          = nullptr;
        const size_t buffer_capacity_ = 0;
        const size_t buffer_mask_     = 0; /* valid if capacity is power of two, else 0 */

        cell& get_cell( size_t pos )
        {
            size_t index = buffer_mask_ ? ( pos & buffer_mask_ ) : ( pos % buffer_capacity_ );
            return buffer_[ index ];
        }
    };

    typedef std::conditional_t< has_capacity, compile_time_storage, runtime_storage > storage_type;

    struct implementation_defined
    {
        typedef cell_allocator allocator;
        typedef std::size_t    size_type;
    };

#endif

public:
    typedef T                                          value_type;
    typedef typename implementation_defined::allocator allocator;
    typedef typename implementation_defined::size_type size_type;

    /** \returns true if the implementation is lock-free.
     *
     *  With single-producer and single-consumer the queue is
     *  wait-free (no CAS used).  For multi-consumer or
     *  multi-producer configurations only the atomic positions
     *  used with CAS are checked.
     */
    bool is_lock_free() const
    {
        return is_lock_free_impl( producer_tag(), consumer_tag() );
    }

    // ---- constructors ----

    /** Construct a queue with compile-time capacity.
     *
     *  Storage is allocated in a static buffer; no heap
     *  allocation is performed.
     *
     *  \pre Must specify a \c capacity<> argument >= 2.
     */
    bounded_ticket_queue()
#if !defined( BOOST_NO_CXX20_HDR_CONCEPTS )
        requires( has_capacity )
#endif
    {}

    /** Construct a queue with runtime capacity.
     *
     *  \param n Queue capacity. Must be >= 2. Not required to be
     *           a power of two, but power-of-two sizes use a
     *           fast masking path.
     *
     *  \pre Must \b not specify a \c capacity<> argument.
     */
#if !defined( BOOST_NO_CXX20_HDR_CONCEPTS )
    explicit bounded_ticket_queue( size_type n )
        requires( !has_capacity )
#else
    template < typename Enabler = std::enable_if< !has_capacity > >
    explicit bounded_ticket_queue( size_type n )
#endif
        :
        storage_( n )
    {}

    /** Construct a queue with runtime capacity and custom allocator.
     *
     *  \param n Queue capacity. Must be >= 2.
     *  \param alloc Allocator used for the cell storage. Its
     *               \c value_type is rebound to \c cell.
     *
     *  \pre Must \b not specify a \c capacity<> argument.
     */
#if !defined( BOOST_NO_CXX20_HDR_CONCEPTS )
    bounded_ticket_queue( size_type n, allocator const& alloc )
        requires( !has_capacity )
#else
    template < typename Enabler = std::enable_if< !has_capacity > >
    bounded_ticket_queue( size_type n, allocator const& alloc )
#endif
        :
        storage_( n, alloc )
    {}

    bounded_ticket_queue( const bounded_ticket_queue& )            = delete;
    bounded_ticket_queue& operator=( const bounded_ticket_queue& ) = delete;
    bounded_ticket_queue( bounded_ticket_queue&& )                 = delete;
    bounded_ticket_queue& operator=( bounded_ticket_queue&& )      = delete;

    /** Destroys the queue.
     */
    ~bounded_ticket_queue()
    {
        consume_all( []( T&& ) {} );
    }

    // ---- capacity / empty ----

    /** Check whether the queue is empty.
     *
     *  \returns true if the queue is empty, false otherwise.
     *
     *  \note The result is only accurate when no other thread
     *  modifies the queue concurrently.
     */
    bool empty() const
    {
        size_t dp = dequeue_pos_.load( detail::memory_order_acquire );
        size_t ep = enqueue_pos_.load( detail::memory_order_acquire );
        return dp == ep;
    }

    /** \returns the capacity of the queue.
     */
    size_type capacity() const
    {
        return storage_.buffer_capacity_;
    }

    // ---- push ----

    /** Pushes a value to the queue.
     *
     *  \returns true if the value was pushed, false if the queue
     *  is full.
     *
     *  \note Thread-safe. Multiple producers may call push
     *  concurrently. Never allocates memory.
     */
    bool push( const T& t )
    {
        return do_push( t );
    }

    bool push( T&& t )
    {
        return do_push( std::move( t ) );
    }

    // ---- pop ----

    /** Pops an element from the queue.
     *
     *  \returns true if an element was popped, false if the queue
     *  was empty.
     */
    bool pop( T& ret )
    {
        return pop< T >( ret );
    }

    /** Pops an element from the queue.
     *
     *  \tparam U type to receive the popped value; \c U must be
     *            constructible from \c T&& and assignable from
     *            \c U.
     *  \returns true if an element was popped, false if the queue
     *           was empty.
     */
#if !defined( BOOST_NO_CXX20_HDR_CONCEPTS )
    template < typename U >
        requires( std::is_constructible< U, T && >::value )
#else
    template < typename U, typename Enabler = typename std::enable_if< std::is_constructible< U, T&& >::value >::type >
#endif
    bool pop( U& ret )
    {
        return consume_one( [ & ]( T&& arg ) {
            ret = U( std::forward< T >( arg ) );
        } );
    }

#if !defined( BOOST_NO_CXX17_HDR_OPTIONAL ) || defined( BOOST_DOXYGEN_INVOKED )
    /** Pops an element, returning a std::optional<T>.
     *
     *  \returns std::optional containing the value on success,
     *          std::nullopt if the queue is empty.
     */
    std::optional< T > pop( uses_optional_t )
    {
        T to_dequeue;
        if ( pop( to_dequeue ) )
            return to_dequeue;
        else
            return std::nullopt;
    }

    /** Pops an element, returning a std::optional.
     *
     *  \tparam U type to receive the popped value; T must be
     *            convertible to U.
     *  \returns std::optional&lt;U&gt; containing the value on
     *           success, std::nullopt if the queue is empty.
     */
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

    // ---- batch consumption ----

    /** Consumes one element via a functor.
     *
     *  Moves one element out of the slot, passes the resulting
     *  rvalue to the functor, and releases the slot for reuse
     *  once the functor returns.
     *
     *  \returns true if an element was consumed.
     */
    template < typename Functor >
    bool consume_one( Functor&& f )
    {
        return consume_one_impl( std::forward< Functor >( f ), consumer_tag() );
    }

    /** Consumes all elements via a functor.
     *
     *  Sequentially pops all elements and applies the functor to
     *  each.
     *
     *  \returns the number of elements consumed.
     */
    template < typename Functor >
    size_t consume_all( Functor&& f )
    {
        size_t element_count = 0;
        while ( consume_one( f ) )
            element_count += 1;

        return element_count;
    }

private:
#ifndef BOOST_DOXYGEN_INVOKED

    // ---- push implementations ----

    template < typename U >
    bool do_push_impl( U&& t, detail::multi_producer_tag )
    {
        size_t pos = enqueue_pos_.load( detail::memory_order_relaxed );

        for ( ;; ) {
            cell&    c   = storage_.get_cell( pos );
            size_t   seq = c.sequence.load( detail::memory_order_acquire );
            intptr_t dif = intptr_t( seq ) - intptr_t( pos );

            if ( dif == 0 ) {
                if ( enqueue_pos_.compare_exchange_weak( pos, pos + 1, detail::memory_order_relaxed ) ) {
                    c.construct_from( std::forward< U >( t ), pos + 1 );
                    return true;
                }
            } else if ( dif < 0 ) {
                return false;
            } else {
                pos = enqueue_pos_.load( detail::memory_order_relaxed );
            }
        }
    }

    template < typename U >
    bool do_push_impl( U&& t, detail::single_producer_tag )
    {
        size_t   pos = enqueue_pos_.load( detail::memory_order_relaxed );
        cell&    c   = storage_.get_cell( pos );
        size_t   seq = c.sequence.load( detail::memory_order_acquire );
        intptr_t dif = intptr_t( seq ) - intptr_t( pos );

        if ( dif == 0 ) {
            enqueue_pos_.store( pos + 1, detail::memory_order_relaxed );
            c.construct_from( std::forward< U >( t ), pos + 1 );
            return true;
        }

        return false;
    }

    template < typename U >
    bool do_push( U&& t )
    {
        return do_push_impl( std::forward< U >( t ), producer_tag() );
    }

    // ---- consume_one implementations ----

    template < typename Functor >
    bool consume_one_impl( Functor&& f, detail::multi_consumer_tag )
    {
        size_t pos = dequeue_pos_.load( detail::memory_order_relaxed );

        for ( ;; ) {
            cell&    c   = storage_.get_cell( pos );
            size_t   seq = c.sequence.load( detail::memory_order_acquire );
            intptr_t dif = intptr_t( seq ) - intptr_t( pos + 1 );

            if ( dif == 0 ) {
                if ( dequeue_pos_.compare_exchange_weak( pos, pos + 1, detail::memory_order_relaxed ) ) {
                    f( c.take() );

                    c.sequence.store( pos + storage_.buffer_capacity_, detail::memory_order_release );
                    return true;
                }
            } else if ( dif < 0 ) {
                return false;
            } else {
                pos = dequeue_pos_.load( detail::memory_order_relaxed );
            }
        }
    }

    template < typename Functor >
    bool consume_one_impl( Functor&& f, detail::single_consumer_tag )
    {
        size_t pos = dequeue_pos_.load( detail::memory_order_relaxed );
        cell&  c   = storage_.get_cell( pos );
        size_t seq = c.sequence.load( detail::memory_order_acquire );

        if ( seq != pos + 1 )
            return false;

        f( c.take() );

        c.sequence.store( pos + storage_.buffer_capacity_, detail::memory_order_release );
        dequeue_pos_.store( pos + 1, detail::memory_order_relaxed );
        return true;
    }

    // ---- is_lock_free implementations ----

    bool is_lock_free_impl( detail::single_producer_tag, detail::single_consumer_tag ) const
    {
        return true;
    }

    bool is_lock_free_impl( detail::single_producer_tag, detail::multi_consumer_tag ) const
    {
        return dequeue_pos_.is_lock_free();
    }

    bool is_lock_free_impl( detail::multi_producer_tag, detail::single_consumer_tag ) const
    {
        return enqueue_pos_.is_lock_free();
    }

    bool is_lock_free_impl( detail::multi_producer_tag, detail::multi_consumer_tag ) const
    {
        return enqueue_pos_.is_lock_free() && dequeue_pos_.is_lock_free();
    }

    // ---- data members ----

    alignas( detail::cacheline_bytes ) storage_type storage_;
    alignas( detail::cacheline_bytes ) detail::atomic< size_t > enqueue_pos_ { 0 };
    alignas( detail::cacheline_bytes ) detail::atomic< size_t > dequeue_pos_ { 0 };
#endif
};

}} // namespace boost::lockfree

#if defined( _MSC_VER )
#    pragma warning( pop )
#endif

#endif /* BOOST_LOCKFREE_BOUNDED_TICKET_QUEUE_HPP_INCLUDED */
