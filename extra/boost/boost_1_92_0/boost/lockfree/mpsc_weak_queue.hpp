//  lock-free multi-producer/single-consumer queue
//  based on Dmitry Vyukov's MPSC queue algorithm
//
//  Copyright (C) 2026 Tim Blechmann
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_LOCKFREE_MPSC_WEAK_QUEUE_HPP_INCLUDED
#define BOOST_LOCKFREE_MPSC_WEAK_QUEUE_HPP_INCLUDED

#include <boost/config.hpp>
#ifdef BOOST_HAS_PRAGMA_ONCE
#    pragma once
#endif

#include <boost/assert.hpp>
#include <boost/core/allocator_access.hpp>
#include <boost/parameter/optional.hpp>
#include <boost/parameter/parameters.hpp>
#include <boost/static_assert.hpp>

#include <boost/lockfree/detail/atomic.hpp>
#include <boost/lockfree/detail/freelist.hpp>
#include <boost/lockfree/detail/parameter.hpp>
#include <boost/lockfree/detail/uses_optional.hpp>
#include <boost/lockfree/lockfree_forward.hpp>

#include <type_traits>


#if defined( _MSC_VER )
#    pragma warning( push )
#    pragma warning( disable : 4324 ) // structure was padded due to __declspec(align())
#endif

#if defined( BOOST_INTEL ) && ( BOOST_INTEL_CXX_VERSION > 1000 )
#    pragma warning( push )
#    pragma warning( disable : 488 ) // template parameter unused in declaring parameter types
#endif


namespace boost { namespace lockfree {

#ifndef BOOST_DOXYGEN_INVOKED
namespace detail {

typedef parameter::parameters< boost::parameter::optional< tag::allocator >,
                               boost::parameter::optional< tag::capacity >,
                               boost::parameter::optional< tag::freelist >,
                               boost::parameter::optional< tag::fixed_sized > >
    mpsc_weak_queue_signature;

} /* namespace detail */
#endif


/** Multi-producer/single-consumer queue based on Dmitry Vyukov's MPSC algorithm.
 *
 *  The push and pop methods execute in a bounded number of instructions
 *  (wait-free execution), but the queue as a whole is not lock-free in
 *  the strict sense (see Limitations).
 *
 *  By default, a freelist manages node memory. Freed nodes are recycled
 *  internally rather than returned to the OS until the queue is destroyed.
 *
 *  \par Limitations
 *  A good writeup of the limitations of this algorithm is available at
 *  <a href="https://int08h.com/post/ode-to-a-vyukov-queue/">Ode to a Vyukov Queue</a>.
 *
 *  - \b Push atomicity: push exchanges the tail then links the previous
 *    node. If a producer thread terminates or stalls between these steps,
 *    the queue becomes inconsistent. The consumer will see a broken link
 *    and stop delivering elements until the producer completes the link.
 *    If the producer permanently terminates, the queue is permanently broken.
 *  - \b Non-linearizable: per-producer FIFO is preserved, but no global
 *    ordering across concurrent producers exists. pop may return false
 *    even if the queue is non-empty when a concurrent push is in progress.
 *
 *  \par Policies
 *  - \ref boost::lockfree::freelist (default \c true):\n
 *    When enabled, freed nodes enter an internal freelist for reuse,
 *    avoiding repeated allocation. When disabled, the allocator handles
 *    every allocation and deallocation. Allocator operations may not be
 *    lock-free.
 *  - \ref boost::lockfree::fixed_sized (default \c false):\n
 *    When enabled, push never performs dynamic allocation, guaranteeing
 *    lock-free behavior. Nodes reside in a fixed array addressed by
 *    index. Capacity is limited to the index type's range (typically
 *    2^16-2). This is required for lock-free operation on platforms
 *    lacking double-width CAS.
 *  - \ref boost::lockfree::capacity (optional):\n
 *    Sets queue size at compile time. Implies \c fixed_sized<true>.
 *  - \ref boost::lockfree::allocator (default \c std::allocator<void>).
 *
 *  \par Requirements
 *  - T must be move-constructible.
 *  - T must be move-assignable.
 *
 *  \note Only one consumer thread is supported. Multiple producer threads
 *  are supported.
 */
template < typename T, typename... Options >
#if !defined( BOOST_NO_CXX20_HDR_CONCEPTS )
    requires( std::is_move_constructible_v< T >, std::is_move_assignable_v< T > )
#endif
class mpsc_weak_queue
{
private:
#ifndef BOOST_DOXYGEN_INVOKED

    typedef typename detail::mpsc_weak_queue_signature::bind< Options... >::type bound_args;

    static constexpr bool use_freelist_default = true;
    static constexpr bool use_freelist         = detail::extract_freelist< bound_args, use_freelist_default >::value;

    static constexpr bool   has_capacity = detail::extract_capacity< bound_args >::has_capacity;
    static constexpr size_t capacity
        = detail::extract_capacity< bound_args >::capacity + 1; // the queue uses one dummy node
    static constexpr bool fixed_sized        = detail::extract_fixed_sized< bound_args >::value;
    static constexpr bool node_based         = !use_freelist || !( has_capacity || fixed_sized );
    static constexpr bool compile_time_sized = use_freelist && has_capacity;

    static constexpr bool capacity_without_freelist = has_capacity && !use_freelist;
    static_assert( capacity_without_freelist == false,
                   "capacity<> argument cannot be used without freelist<> argument" );

    static constexpr bool can_reserve = use_freelist;

    struct BOOST_MAY_ALIAS node
    {
        typedef typename detail::select_tagged_handle< node, node_based >::handle_type handle_type;

        template < typename TagT >
        node( T const& v, handle_type null_handle, TagT /*next_tag*/ ) :
            next( null_handle ),
            data( v )
        {}

        template < typename TagT >
        node( T&& v, handle_type null_handle, TagT /*next_tag*/ ) :
            next( null_handle ),
            data( std::move( v ) )
        {}

        template < typename TagT >
        node( handle_type null_handle, TagT /*next_tag*/ ) :
            next( null_handle )
        {}

        atomic< handle_type > next;
        T                     data;
    };

    typedef detail::extract_allocator_t< bound_args, node > node_allocator;
    typedef std::conditional_t< use_freelist,
                                detail::select_freelist_t< node, node_allocator, compile_time_sized, fixed_sized, capacity >,
                                detail::direct_allocator< node, node_allocator > >
                                                                                   pool_t;
    typedef typename pool_t::tagged_node_handle                                    tagged_node_handle;
    typedef typename detail::select_tagged_handle< node, node_based >::handle_type handle_type;

    void initialize()
    {
        node*       n            = pool.template construct< true, false >( pool.null_handle() );
        handle_type dummy_handle = pool.get_handle( n );
        tail_                    = dummy_handle;
        head_.store( dummy_handle, memory_order_release );
    }

    struct implementation_defined
    {
        typedef node_allocator allocator;
        typedef std::size_t    size_type;
    };

#endif

public:
    typedef T                                          value_type;
    typedef typename implementation_defined::allocator allocator;
    typedef typename implementation_defined::size_type size_type;

    /**
     * \returns true if the implementation is lock-free.
     *
     * \warning Only checks whether the head, tail, and freelist can be
     * modified in a lock-free manner. On most platforms the entire
     * implementation is lock-free when this returns true.
     */
    bool is_lock_free() const
    {
        return head_.is_lock_free() && pool.is_lock_free();
    }

    /** Construct a queue.
     *
     *  Requires a capacity<> argument when freelist is enabled.
     */
    mpsc_weak_queue()
#if !defined( BOOST_NO_CXX20_HDR_CONCEPTS )
        requires( has_capacity || !use_freelist )
#endif
        :
        pool( node_allocator(), capacity )
    {
        BOOST_ASSERT( has_capacity || !use_freelist );
        initialize();
    }

    /** Construct a fixed-sized queue with a custom allocator.
     *
     *  Requires a capacity<> argument when freelist is enabled.
     */
#if !defined( BOOST_NO_CXX20_HDR_CONCEPTS )
    template < typename U >
        requires( has_capacity || !use_freelist )
#else
    template < typename U, typename Enabler = std::enable_if< has_capacity || !use_freelist > >
#endif
    explicit mpsc_weak_queue( typename boost::allocator_rebind< node_allocator, U >::type const& alloc ) :
        pool( alloc, capacity )
    {
        initialize();
    }

    /** Construct a fixed-sized queue with a custom allocator.
     *
     *  Requires a capacity<> argument when freelist is enabled.
     */
#if !defined( BOOST_NO_CXX20_HDR_CONCEPTS )
    explicit mpsc_weak_queue( allocator const& alloc )
        requires( has_capacity || !use_freelist )
#else
    template < typename Enabler = std::enable_if< has_capacity || !use_freelist > >
    explicit mpsc_weak_queue( allocator const& alloc )
#endif
        :
        pool( alloc, capacity )
    {
        initialize();
    }

    /** Construct a variable-sized queue.
     *
     *  Allocates \c n nodes initially for the freelist.
     *
     *  Requires that no capacity<> argument is specified and that
     *  freelist is enabled.
     */
#if !defined( BOOST_NO_CXX20_HDR_CONCEPTS )
    explicit mpsc_weak_queue( size_type n )
        requires( !has_capacity && use_freelist )
#else
    template < typename Enabler = std::enable_if< !has_capacity && use_freelist > >
    explicit mpsc_weak_queue( size_type n )
#endif
        :
        pool( node_allocator(), n + 1 )
    {
        initialize();
    }

    /** Construct a variable-sized queue with a custom allocator.
     *
     *  Allocates \c n nodes initially for the freelist.
     *
     *  Requires that no capacity<> argument is specified and that
     *  freelist is enabled.
     */
#if !defined( BOOST_NO_CXX20_HDR_CONCEPTS )
    mpsc_weak_queue( size_type n, allocator const& alloc )
        requires( !has_capacity && use_freelist )
#else
    template < typename Enabler = std::enable_if< !has_capacity && use_freelist > >
    mpsc_weak_queue( size_type n, allocator const& alloc )
#endif
        :
        pool( alloc, n + 1 )
    {
        initialize();
    }

    mpsc_weak_queue( const mpsc_weak_queue& )            = delete;
    mpsc_weak_queue& operator=( const mpsc_weak_queue& ) = delete;
    mpsc_weak_queue( mpsc_weak_queue&& )                 = delete;
    mpsc_weak_queue& operator=( mpsc_weak_queue&& )      = delete;

    /** \copydoc boost::lockfree::stack::reserve
     * */
#if !defined( BOOST_NO_CXX20_HDR_CONCEPTS )
    void reserve( size_type n )
        requires( can_reserve )
#else
    template < typename Enabler = std::enable_if< can_reserve > >
    void reserve( size_type n )
#endif
    {
        pool.template reserve< true >( n );
    }

    /** \copydoc boost::lockfree::stack::reserve_unsafe
     * */
#if !defined( BOOST_NO_CXX20_HDR_CONCEPTS )
    void reserve_unsafe( size_type n )
        requires( can_reserve )
#else
    template < typename Enabler = std::enable_if< can_reserve > >
    void reserve_unsafe( size_type n )
#endif
    {
        pool.template reserve< false >( n );
    }

    /** Destroys the queue and frees all nodes from the freelist.
     */
    ~mpsc_weak_queue()
    {
        consume_all( []( T&& ) {} );

        pool.template destruct< false >( head_.load( memory_order_relaxed ) );
    }

    /** Check whether the queue is empty.
     *
     * \returns true if the queue is empty, false otherwise.
     *
     * \note The result is only accurate when no other thread modifies the
     * queue concurrently.
     */
    bool empty() const
    {
        return head_.load( memory_order_acquire ) == tail_;
    }

    /** Pushes a value to the queue.
     *
     * \returns true if the value was pushed, false if node allocation failed.
     *
     * \note Thread-safe. Multiple producers may call push concurrently.
     *       When the memory pool is exhausted and the pool is not fixed-sized,
     *       a new node is allocated via the allocator, which may not be
     *       lock-free.
     */
    bool push( const T& t )
    {
        return do_push< false >( t );
    }

    /// \copydoc boost::lockfree::mpsc_weak_queue::push(const T & t)
    bool push( T&& t )
    {
        return do_push< false >( std::forward< T >( t ) );
    }

    /** Pushes a value to the queue without allocating.
     *
     * \returns true if the value was pushed, false if the freelist is empty.
     *
     * \note Thread-safe. Multiple producers may call bounded_push
     *       concurrently. Unlike push, bounded_push never allocates memory;
     *       it fails when the freelist is exhausted.
     * \throws std::bad_alloc if the allocator throws during node
     *         construction.
     */
#if !defined( BOOST_NO_CXX20_HDR_CONCEPTS )
    bool bounded_push( const T& t )
        requires( use_freelist )
#else
    template < typename Enabler = std::enable_if< use_freelist > >
    bool bounded_push( const T& t )
#endif
    {
        return do_push< true >( t );
    }

    /// \copydoc boost::lockfree::mpsc_weak_queue::bounded_push(const T & t)
#if !defined( BOOST_NO_CXX20_HDR_CONCEPTS )
    bool bounded_push( T&& t )
        requires( use_freelist )
#else
    template < typename Enabler = std::enable_if< use_freelist > >
    bool bounded_push( T&& t )
#endif
    {
        return do_push< true >( std::forward< T >( t ) );
    }


private:
#ifndef BOOST_DOXYGEN_INVOKED
    template < bool Bounded >
    bool do_push( T&& t )
    {
        node* n = pool.template construct< true, Bounded >( std::forward< T >( t ), pool.null_handle() );
        return do_push_node( n );
    }

    template < bool Bounded >
    bool do_push( T const& t )
    {
        node* n = pool.template construct< true, Bounded >( t, pool.null_handle() );
        return do_push_node( n );
    }

    bool do_push_node( node* n )
    {
        if ( n == nullptr )
            return false;

        handle_type node_handle = pool.get_handle( n );

        handle_type old_head     = head_.exchange( node_handle, memory_order_acq_rel );
        node*       old_head_ptr = pool.get_pointer( old_head );
        old_head_ptr->next.store( node_handle, memory_order_release );
        return true;
    }
#endif

public:
    /** Pushes a value to the queue without synchronization.
     *
     * \returns true if the value was pushed, false if node allocation failed.
     *
     * \note Not thread-safe. Only one producer thread should call
     *       unsynchronized_push. When the memory pool is exhausted and
     *       the pool is not fixed-sized, a new node is allocated via the
     *       allocator, which may not be lock-free.
     * \throws std::bad_alloc if the allocator throws during node
     *         construction.
     */
    bool unsynchronized_push( const T& t )
    {
        return unsynchronized_push_impl( t );
    }

    /// \copydoc boost::lockfree::mpsc_weak_queue::unsynchronized_push(const T& t)
    bool unsynchronized_push( T&& t )
    {
        return unsynchronized_push_impl( std::forward< T >( t ) );
    }

private:
#ifndef BOOST_DOXYGEN_INVOKED
    template < typename U >
    bool unsynchronized_push_impl( U&& t )
    {
        node* n = pool.template construct< false, false >( std::forward< U >( t ), pool.null_handle() );

        if ( n == nullptr )
            return false;

        handle_type node_handle = pool.get_handle( n );

        handle_type old_head     = head_.load( memory_order_relaxed );
        node*       old_head_ptr = pool.get_pointer( old_head );
        old_head_ptr->next.store( node_handle, memory_order_relaxed );
        head_.store( node_handle, memory_order_release );
        return true;
    }

#endif
public:
    /** Pops an element from the queue.
     *
     * \returns true if an element was popped, false if the queue was empty.
     *
     * \note Thread-safe and wait-free. Only one consumer thread should call
     *       pop. The output argument may be modified even when the operation
     *       fails. pop may return false even if the queue is non-empty when
     *       a concurrent push is in progress (see Limitations).
     */
    bool pop( T& ret )
    {
        return pop< T >( ret );
    }

    /** Pops an element from the queue.
     *
     * \tparam U type to receive the popped value; U must be constructible
     *           from T (explicit or implicit).
     * \returns true if an element was popped, false if the queue was empty.
     *
     * \note Thread-safe and wait-free. Only one consumer thread should call
     *       pop. The output argument may be modified even when the operation
     *       fails. pop may return false even if the queue is non-empty when
     *       a concurrent push is in progress (see Limitations).
     */
#if !defined( BOOST_NO_CXX20_HDR_CONCEPTS )
    template < typename U >
        requires( std::is_constructible_v< U, T && > )
#else
    template < typename U, typename Enabler = std::enable_if_t< std::is_constructible< U, T&& >::value > >
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
     * \returns std::optional containing the value on success,
     *          std::nullopt if the queue is empty.
     *
     * \note Thread-safe and wait-free. Only one consumer thread should call
     *       this overload.
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
     * \tparam U type to receive the popped value; T must be convertible to U.
     * \returns std::optional&lt;U&gt; containing the value on success,
     *          std::nullopt if the queue is empty.
     *
     * \note Thread-safe and wait-free. Only one consumer thread should call
     *       this overload.
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

    /** Pops an element without synchronization.
     *
     * \returns true if an element was popped, false if the queue was empty.
     *
     * \note Not thread-safe but wait-free. Only one consumer thread should
     *       call unsynchronized_pop. The output argument may be modified even
     *       when the operation fails.
     */
    bool unsynchronized_pop( T& ret )
    {
        return unsynchronized_pop< T >( ret );
    }

    /** Pops an element without synchronization.
     *
     * \tparam U type to receive the popped value; U must be constructible
     *           from T (explicit or implicit).
     * \returns true if an element was popped, false if the queue was empty.
     *
     * \note Not thread-safe but wait-free. Only one consumer thread should
     *       call unsynchronized_pop.
     */
#if !defined( BOOST_NO_CXX20_HDR_CONCEPTS )
    template < typename U >
        requires( std::is_constructible_v< U, T && > )
#else
    template < typename U, typename Enabler = std::enable_if_t< std::is_constructible< U, T&& >::value > >
#endif
    bool unsynchronized_pop( U& ret )
    {
        return unsynchronized_consume_one( [ & ]( T&& arg ) {
            ret = U( std::forward< T >( arg ) );
        } );
    }

    /** Consumes one element via a functor.
     *
     *  Pops one element and applies the functor to it.
     *
     * \returns true if an element was consumed.
     *
     * \note Thread-safe and wait-free when the functor is thread-safe and
     *       non-blocking. Only one consumer thread should call this.
     */
    template < typename Functor >
    bool consume_one( Functor&& f )
    {
        handle_type tail     = tail_;
        node*       tail_ptr = pool.get_pointer( tail );
        handle_type next     = tail_ptr->next.load( memory_order_acquire );
        node*       next_ptr = pool.get_pointer( next );

        if ( next_ptr == 0 ) {
            handle_type head = head_.load( memory_order_acquire );
            if ( tail == head )
                return false;

            return false;
        }

        f( std::move( next_ptr->data ) );

        tail_ = next;
        pool.template destruct< true >( tail );
        return true;
    }

    /** Consumes one element via a functor without synchronization.
     *
     *  Pops one element and applies the functor to it.
     *
     * \returns true if an element was consumed.
     *
     * \note Not thread-safe but wait-free. Only one consumer thread should
     *       call this.
     */
    template < typename Functor >
    bool unsynchronized_consume_one( Functor&& f )
    {
        handle_type tail     = tail_;
        node*       tail_ptr = pool.get_pointer( tail );
        handle_type next     = tail_ptr->next.load( memory_order_relaxed );
        node*       next_ptr = pool.get_pointer( next );

        if ( next_ptr == 0 ) {
            handle_type head = head_.load( memory_order_relaxed );
            if ( tail == head )
                return false;

            return false;
        }

        f( std::move( next_ptr->data ) );

        tail_ = next;
        pool.template destruct< false >( tail );
        return true;
    }

    /** Consumes all elements via a functor.
     *
     *  Sequentially pops all elements and applies the functor to each.
     *
     * \returns the number of elements consumed.
     *
     * \note Thread-safe and wait-free when the functor is thread-safe and
     *       non-blocking. Only one consumer thread should call this.
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
    atomic< handle_type > head_ {};
    alignas( detail::cacheline_bytes ) handle_type tail_ {};

    pool_t pool;
#endif
};

}} // namespace boost::lockfree

#if ( defined( BOOST_INTEL ) && ( BOOST_INTEL_CXX_VERSION > 1000 ) ) || defined( _MSC_VER )
#    pragma warning( pop )
#endif

#endif /* BOOST_LOCKFREE_MPSC_QUEUE_HPP_INCLUDED */
