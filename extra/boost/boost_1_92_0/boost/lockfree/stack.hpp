//  Copyright (C) 2008-2013 Tim Blechmann
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_LOCKFREE_STACK_HPP_INCLUDED
#define BOOST_LOCKFREE_STACK_HPP_INCLUDED

#include <boost/config.hpp>
#ifdef BOOST_HAS_PRAGMA_ONCE
#    pragma once
#endif


#include <boost/assert.hpp>
#include <boost/core/allocator_access.hpp>
#include <boost/core/no_exceptions_support.hpp>
#include <boost/core/span.hpp>
#include <boost/parameter/optional.hpp>
#include <boost/parameter/parameters.hpp>
#include <boost/static_assert.hpp>

#include <boost/lockfree/detail/atomic.hpp>
#include <boost/lockfree/detail/freelist.hpp>
#include <boost/lockfree/detail/parameter.hpp>
#include <boost/lockfree/detail/tagged_ptr.hpp>
#include <boost/lockfree/detail/uses_optional.hpp>
#include <boost/lockfree/lockfree_forward.hpp>

#include <cstdint>
#include <tuple>
#include <type_traits>

namespace boost { namespace lockfree {
namespace detail {

typedef parameter::parameters< boost::parameter::optional< tag::allocator >, boost::parameter::optional< tag::capacity > >
    stack_signature;

} // namespace detail

/** The stack class provides a multi-writer/multi-reader stack, pushing and popping is lock-free,
 *  construction/destruction has to be synchronized. It uses a freelist for memory management,
 *  freed nodes are pushed to the freelist and not returned to the OS before the stack is destroyed.
 *
 *  \b Policies:
 *
 *  - \c boost::lockfree::fixed_sized<>, defaults to \c boost::lockfree::fixed_sized<false> <br>
 *    Can be used to completely disable dynamic memory allocations during push in order to ensure lockfree behavior.<br>
 *    If the data structure is configured as fixed-sized, the internal nodes are stored inside an array and they are
 * addressed by array indexing. This limits the possible size of the stack to the number of elements that can be
 * addressed by the index type (usually 2**16-2), but on platforms that lack double-width compare-and-exchange
 * instructions, this is the best way to achieve lock-freedom.
 *
 *  - \c boost::lockfree::capacity<>, optional <br>
 *    If this template argument is passed to the options, the size of the stack is set at compile-time. <br>
 *    It this option implies \c fixed_sized<true>
 *
 *  - \c boost::lockfree::allocator<>, defaults to \c boost::lockfree::allocator<std::allocator<void>> <br>
 *    Specifies the allocator that is used for the internal freelist
 *
 *  \b Requirements:
 *  - T must have a copy constructor or a move constructor
 * */
template < typename T, typename... Options >
#if !defined( BOOST_NO_CXX20_HDR_CONCEPTS )
    requires( std::is_copy_assignable_v< T > || std::is_move_assignable_v< T > )
#endif
class stack
{
private:
#ifndef BOOST_DOXYGEN_INVOKED
    BOOST_STATIC_ASSERT( std::is_copy_constructible< T >::value || std::is_move_constructible< T >::value );

    typedef typename detail::stack_signature::bind< Options... >::type bound_args;

    static const bool   has_capacity       = detail::extract_capacity< bound_args >::has_capacity;
    static const size_t capacity           = detail::extract_capacity< bound_args >::capacity;
    static const bool   fixed_sized        = detail::extract_fixed_sized< bound_args >::value;
    static const bool   node_based         = !( has_capacity || fixed_sized );
    static const bool   compile_time_sized = has_capacity;

    struct BOOST_MAY_ALIAS node
    {
        template < typename TagT >
        node( const T& val, TagT /*next_tag*/ ) :
            v( val )
        {}

        template < typename TagT >
        node( T&& val, TagT /*next_tag*/ ) :
            v( std::forward< T >( val ) )
        {}

        node() = delete;

        typedef typename detail::select_tagged_handle< node, node_based >::handle_type handle_t;

        handle_t next;
        T        v;
    };

    typedef typename detail::extract_allocator< bound_args, node >::type node_allocator;
    typedef
        typename detail::select_freelist< node, node_allocator, compile_time_sized, fixed_sized, capacity >::type pool_t;
    typedef typename pool_t::tagged_node_handle tagged_node_handle;

    // check compile-time capacity
    static constexpr bool capacity_is_valid = has_capacity ? capacity - 1 < std::numeric_limits< std::uint16_t >::max()
                                                           : true;
    BOOST_STATIC_ASSERT( capacity_is_valid );

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
     * \return true, if implementation is lock-free.
     *
     * \warning It only checks, if the top stack node and the freelist can be modified in a lock-free manner.
     *          On most platforms, the whole implementation is lock-free, if this is true. Using c++0x-style atomics,
     *          there is no possibility to provide a completely accurate implementation, because one would need to test
     *          every internal node, which is impossible if further nodes will be allocated from the operating system.
     *
     * */
    bool is_lock_free( void ) const
    {
        return tos.is_lock_free() && pool.is_lock_free();
    }

    /** Construct a fixed-sized stack
     *
     *  \pre Must specify a capacity<> argument
     * */
    explicit stack( void )
#if !defined( BOOST_NO_CXX20_HDR_CONCEPTS )
        requires( has_capacity )
#endif
        :
        pool( node_allocator(), capacity )
    {
        // Don't use BOOST_STATIC_ASSERT() here since it will be evaluated when compiling
        // this function and this function may be compiled even when it isn't being used.
        BOOST_ASSERT( has_capacity );
        initialize();
    }

    /** Construct a fixed-sized stack with a custom allocator
     *
     *  \pre Must specify a capacity<> argument
     * */
#if !defined( BOOST_NO_CXX20_HDR_CONCEPTS )
    template < typename U >
        requires( has_capacity )
#else
    template < typename U, typename Enabler = std::enable_if< has_capacity > >
#endif
    explicit stack( typename boost::allocator_rebind< node_allocator, U >::type const& alloc ) :
        pool( alloc, capacity )
    {
        initialize();
    }

    /** Construct a fixed-sized stack with a custom allocator
     *
     *  \pre Must specify a capacity<> argument
     * */
#if !defined( BOOST_NO_CXX20_HDR_CONCEPTS )
    explicit stack( allocator const& alloc )
        requires( has_capacity )
        :
#else
    template < typename Enabler = std::enable_if< has_capacity > >
    explicit stack( allocator const& alloc ) :
#endif
        pool( alloc, capacity )
    {
        initialize();
    }

    /** Construct a variable-sized stack
     *
     *  Allocate n nodes initially for the freelist
     *
     *  \pre Must \b not specify a capacity<> argument
     * */
#if !defined( BOOST_NO_CXX20_HDR_CONCEPTS )
    explicit stack( size_type n )
        requires( !has_capacity )
        :
#else
    template < typename Enabler = std::enable_if< !has_capacity > >
    explicit stack( size_type n ) :
#endif
        pool( node_allocator(), n )
    {
        initialize();
    }

    stack( const stack& )            = delete;
    stack& operator=( const stack& ) = delete;
    stack( stack&& )                 = delete;
    stack& operator=( stack&& )      = delete;


    /** Construct a variable-sized stack with a custom allocator
     *
     *  Allocate n nodes initially for the freelist
     *
     *  \pre Must \b not specify a capacity<> argument
     * */
#if !defined( BOOST_NO_CXX20_HDR_CONCEPTS )
    template < typename U >
        requires( !has_capacity )
#else
    template < typename U, typename Enabler = std::enable_if< !has_capacity > >
#endif
    stack( size_type n, typename boost::allocator_rebind< node_allocator, U >::type const& alloc ) :
        pool( alloc, n )
    {
        initialize();
    }

    /** Construct a variable-sized stack with a custom allocator
     *
     *  Allocate n nodes initially for the freelist
     *
     *  \pre Must \b not specify a capacity<> argument
     * */
#if !defined( BOOST_NO_CXX20_HDR_CONCEPTS )
    stack( size_type n, node_allocator const& alloc )
        requires( !has_capacity )
        :
#else
    template < typename Enabler = std::enable_if< !has_capacity > >
    stack( size_type n, node_allocator const& alloc ) :
#endif
        pool( alloc, n )
    {
        initialize();
    }

    /** Allocate n nodes for freelist
     *
     *  \pre  only valid if no capacity<> argument given
     *  \note thread-safe, may block if memory allocator blocks
     *
     * */
#if !defined( BOOST_NO_CXX20_HDR_CONCEPTS )
    void reserve( size_type n )
        requires( !has_capacity )
#else
    template < typename Enabler = std::enable_if< !has_capacity > >
    void reserve( size_type n )
#endif
    {
        pool.template reserve< true >( n );
    }

    /** Allocate n nodes for freelist
     *
     *  \pre  only valid if no capacity<> argument given
     *  \note not thread-safe, may block if memory allocator blocks
     *
     * */
#if !defined( BOOST_NO_CXX20_HDR_CONCEPTS )
    void reserve_unsafe( size_type n )
        requires( !has_capacity )
#else
    template < typename Enabler = std::enable_if< !has_capacity > >
    void reserve_unsafe( size_type n )
#endif
    {
        pool.template reserve< false >( n );
    }

    /** Destroys stack, free all nodes from freelist.
     *
     *  \note not thread-safe
     *
     * */
    ~stack( void )
    {
        consume_all( []( const T& ) {} );
    }

private:
#ifndef BOOST_DOXYGEN_INVOKED
    void initialize( void )
    {
        tos.store( tagged_node_handle( pool.null_handle(), 0 ) );
    }

    void link_nodes_atomic( node* new_top_node, node* end_node )
    {
        tagged_node_handle old_tos = tos.load( detail::memory_order_relaxed );
        for ( ;; ) {
            tagged_node_handle new_tos( pool.get_handle( new_top_node ), old_tos.get_next_tag() );
            end_node->next = pool.get_handle( old_tos );

            if ( tos.compare_exchange_weak( old_tos, new_tos ) )
                break;
        }
    }

    void link_nodes_unsafe( node* new_top_node, node* end_node )
    {
        tagged_node_handle old_tos = tos.load( detail::memory_order_relaxed );

        tagged_node_handle new_tos( pool.get_handle( new_top_node ), old_tos.get_next_tag() );
        end_node->next = pool.get_handle( old_tos );

        tos.store( new_tos, memory_order_relaxed );
    }

    template < bool Threadsafe, bool Bounded, typename ConstIterator >
    std::tuple< node*, node* > prepare_node_list( ConstIterator begin, ConstIterator end, ConstIterator& ret )
    {
        ConstIterator it       = begin;
        node*         end_node = pool.template construct< Threadsafe, Bounded >( *it++ );
        if ( end_node == NULL ) {
            ret = begin;
            return std::make_tuple< node*, node* >( NULL, NULL );
        }

        node* new_top_node = end_node;
        end_node->next     = NULL;

        BOOST_TRY
        {
            /* link nodes */
            for ( ; it != end; ++it ) {
                node* newnode = pool.template construct< Threadsafe, Bounded >( *it );
                if ( newnode == NULL )
                    break;
                newnode->next = new_top_node;
                new_top_node  = newnode;
            }
        }
        BOOST_CATCH( ... )
        {
            for ( node* current_node = new_top_node; current_node != NULL; ) {
                node* next = current_node->next;
                pool.template destruct< Threadsafe >( current_node );
                current_node = next;
            }
            BOOST_RETHROW;
        }
        BOOST_CATCH_END

        ret = it;
        return std::make_tuple( new_top_node, end_node );
    }

    template < bool Bounded >
    bool do_push( T&& v )
    {
        node* newnode = pool.template construct< true, Bounded >( std::forward< T >( v ) );
        if ( newnode == 0 )
            return false;

        link_nodes_atomic( newnode, newnode );
        return true;
    }

    template < bool Bounded >
    bool do_push( T const& v )
    {
        node* newnode = pool.template construct< true, Bounded >( v );
        if ( newnode == 0 )
            return false;

        link_nodes_atomic( newnode, newnode );
        return true;
    }

    template < bool Bounded, typename ConstIterator >
    ConstIterator do_push( ConstIterator begin, ConstIterator end )
    {
        node*         new_top_node;
        node*         end_node;
        ConstIterator ret;

        std::tie( new_top_node, end_node ) = prepare_node_list< true, Bounded >( begin, end, ret );
        if ( new_top_node )
            link_nodes_atomic( new_top_node, end_node );

        return ret;
    }
#endif

public:
    /** Pushes object t to the stack.
     *
     * \post object will be pushed to the stack, if internal node can be allocated
     * \returns true, if the push operation is successful.
     *
     * \note Thread-safe. If internal memory pool is exhausted and the memory pool is not fixed-sized, a new node will
     * be allocated from the OS. This may not be lock-free. \throws if memory allocator throws
     * */
    bool push( const T& v )
    {
        return do_push< false >( v );
    }

    /// \copydoc boost::lockfree::stack::push(const T& t)
    bool push( T&& v )
    {
        return do_push< false >( std::forward< T >( v ) );
    }

    /** Pushes object t to the stack.
     *
     * \post object will be pushed to the stack, if internal node can be allocated
     * \returns true, if the push operation is successful.
     *
     * \note Thread-safe and non-blocking. If internal memory pool is exhausted, the push operation will fail
     * */
    bool bounded_push( const T& v )
    {
        return do_push< true >( v );
    }

    /// \copydoc boost::lockfree::stack::bounded_push(const T& t)
    bool bounded_push( T&& v )
    {
        return do_push< true >( std::forward< T >( v ) );
    }


    /** Pushes as many objects from the range [begin, end) as freelist node can be allocated.
     *
     * \return iterator to the first element, which has not been pushed
     *
     * \note Operation is applied atomically
     * \note Thread-safe. If internal memory pool is exhausted and the memory pool is not fixed-sized, a new node will
     * be allocated from the OS. This may not be lock-free.
     *
     * \throws if memory allocator throws
     */
    template < typename ConstIterator >
    ConstIterator push( ConstIterator begin, ConstIterator end )
    {
        return do_push< false, ConstIterator >( begin, end );
    }

    /** Pushes as many objects from the span as freelist node can be allocated.
     *
     * \return Number of elements pushed
     *
     * \note Operation is applied atomically
     * \note Thread-safe. If internal memory pool is exhausted and the memory pool is not fixed-sized, a new node will
     * be allocated from the OS. This may not be lock-free.
     *
     * \throws if memory allocator throws
     */
    template < std::size_t Extent >
    size_type push( boost::span< const T, Extent > t )
    {
        const T* end_pushed = push( t.begin(), t.end() );
        return std::distance( t.begin(), end_pushed );
    }

    /** Pushes as many objects from the range [begin, end) as freelist node can be allocated.
     *
     * \return iterator to the first element, which has not been pushed
     *
     * \note Operation is applied atomically
     * \note Thread-safe and non-blocking. If internal memory pool is exhausted, the push operation will fail
     * \throws if memory allocator throws
     */
    template < typename ConstIterator >
    ConstIterator bounded_push( ConstIterator begin, ConstIterator end )
    {
        return do_push< true, ConstIterator >( begin, end );
    }

    /** Pushes as many objects from the span as freelist node can be allocated.
     *
     * \return Number of elements pushed
     *
     * \note Operation is applied atomically
     * \note Thread-safe and non-blocking. If internal memory pool is exhausted, the push operation will fail
     * \throws if memory allocator throws
     */
    template < std::size_t Extent >
    size_type bounded_push( boost::span< const T, Extent > t )
    {
        const T* end_pushed = bounded_push( t.begin(), t.end() );
        return std::distance( t.begin(), end_pushed );
    }


    /** Pushes object t to the stack.
     *
     * \post object will be pushed to the stack, if internal node can be allocated
     * \returns true, if the push operation is successful.
     *
     * \note Not thread-safe. If internal memory pool is exhausted and the memory pool is not fixed-sized, a new node
     * will be allocated from the OS. This may not be lock-free.
     * \throws if memory allocator throws
     * */
    bool unsynchronized_push( const T& v )
    {
        node* newnode = pool.template construct< false, false >( v );
        if ( newnode == 0 )
            return false;

        link_nodes_unsafe( newnode, newnode );
        return true;
    }

    /// \copydoc boost::lockfree::stack::unsynchronized_push(const T& t)
    bool unsynchronized_push( T&& v )
    {
        node* newnode = pool.template construct< false, false >( std::forward< T >( v ) );
        if ( newnode == 0 )
            return false;

        link_nodes_unsafe( newnode, newnode );
        return true;
    }

    /** Pushes as many objects from the range [begin, end) as freelist node can be allocated.
     *
     * \return iterator to the first element, which has not been pushed
     *
     * \note Not thread-safe. If internal memory pool is exhausted and the memory pool is not fixed-sized, a new node
     * will be allocated from the OS. This may not be lock-free.
     * \throws if memory allocator throws
     */
    template < typename ConstIterator >
    ConstIterator unsynchronized_push( ConstIterator begin, ConstIterator end )
    {
        node*         new_top_node;
        node*         end_node;
        ConstIterator ret;

        std::tie( new_top_node, end_node ) = prepare_node_list< false, false >( begin, end, ret );
        if ( new_top_node )
            link_nodes_unsafe( new_top_node, end_node );

        return ret;
    }

    /** Pushes as many objects from the span as freelist node can be allocated.
     *
     * \return iterator to the first element, which has not been pushed
     *
     * \note Not thread-safe. If internal memory pool is exhausted and the memory pool is not fixed-sized, a new node
     * will be allocated from the OS. This may not be lock-free. \throws if memory allocator throws
     */
    template < std::size_t Extent >
    size_type unsynchronized_push( boost::span< const T, Extent > t )
    {
        const T* end_pushed = unsynchronized_push( t.begin(), t.end() );
        return std::distance( t.begin(), end_pushed );
    }

    /** Pops object from stack.
     *
     * \post if pop operation is successful, object will be copied to ret.
     * \returns true, if the pop operation is successful, false if stack was empty.
     *
     * \note Thread-safe and non-blocking
     *
     * */
    bool pop( T& ret )
    {
        return pop< T >( ret );
    }

    /** Pops object from stack.
     *
     * \pre type T must be convertible to U
     * \post if pop operation is successful, object will be copied to ret.
     * \returns true, if the pop operation is successful, false if stack was empty.
     *
     * \note Thread-safe and non-blocking
     *
     * */
#if !defined( BOOST_NO_CXX20_HDR_CONCEPTS )
    template < typename U >
        requires( std::is_convertible_v< T, U > )
#else
    template < typename U, typename Enabler = std::enable_if_t< std::is_convertible< T, U >::value > >
#endif
    bool pop( U& ret )
    {
        return consume_one( [ & ]( T&& arg ) {
            ret = std::forward< T >( arg );
        } );
    }

#if !defined( BOOST_NO_CXX17_HDR_OPTIONAL ) || defined( BOOST_DOXYGEN_INVOKED )
    /** Pops object from stack, returning a std::optional<>
     *
     * \returns `std::optional` with value if successful, `std::nullopt` if stack is empty.
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

    /** Pops object from stack, returning a std::optional<>
     *
     * \pre type T must be convertible to U
     * \returns `std::optional` with value if successful, `std::nullopt` if stack is empty.
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

    /** Pops object from stack.
     *
     * \post if pop operation is successful, object will be copied to ret.
     * \returns true, if the pop operation is successful, false if stack was empty.
     *
     * \note Not thread-safe, but non-blocking
     *
     * */
    bool unsynchronized_pop( T& ret )
    {
        return unsynchronized_pop< T >( ret );
    }

    /** Pops object from stack.
     *
     * \pre type T must be convertible to U
     * \post if pop operation is successful, object will be copied to ret.
     * \returns true, if the pop operation is successful, false if stack was empty.
     *
     * \note Not thread-safe, but non-blocking
     *
     * */
#if !defined( BOOST_NO_CXX20_HDR_CONCEPTS )
    template < typename U >
        requires( std::is_convertible_v< T, U > )
#else
    template < typename U, typename Enabler = std::enable_if_t< std::is_convertible< T, U >::value > >
#endif
    bool unsynchronized_pop( U& ret )
    {
        tagged_node_handle old_tos         = tos.load( detail::memory_order_relaxed );
        node*              old_tos_pointer = pool.get_pointer( old_tos );

        if ( !old_tos_pointer )
            return false;

        node*              new_tos_ptr = pool.get_pointer( old_tos_pointer->next );
        tagged_node_handle new_tos( pool.get_handle( new_tos_ptr ), old_tos.get_next_tag() );

        tos.store( new_tos, memory_order_relaxed );
        ret = std::move( old_tos_pointer->v );
        pool.template destruct< false >( old_tos );
        return true;
    }

    /** consumes one element via a functor
     *
     *  pops one element from the stack and applies the functor on this object
     *
     * \returns true, if one element was consumed
     *
     * \note Thread-safe and non-blocking, if functor is thread-safe and non-blocking
     * */
    template < typename Functor >
    bool consume_one( Functor&& f )
    {
        tagged_node_handle old_tos = tos.load( detail::memory_order_acquire );

        for ( ;; ) {
            node* old_tos_pointer = pool.get_pointer( old_tos );
            if ( !old_tos_pointer )
                return false;

            tagged_node_handle new_tos( old_tos_pointer->next, old_tos.get_next_tag() );

            if ( tos.compare_exchange_weak( old_tos, new_tos ) ) {
                f( std::move( old_tos_pointer->v ) );
                pool.template destruct< true >( old_tos );
                return true;
            }
        }
    }

    /** consumes all elements via a functor
     *
     * sequentially pops all elements from the stack and applies the functor on each object
     *
     * \returns number of elements that are consumed
     *
     * \note Thread-safe and non-blocking, if functor is thread-safe and non-blocking
     * */
    template < typename Functor >
    size_t consume_all( Functor&& f )
    {
        size_t element_count = 0;
        while ( consume_one( f ) )
            element_count += 1;

        return element_count;
    }

    /** consumes all elements via a functor
     *
     * atomically pops all elements from the stack and applies the functor on each object
     *
     * \returns number of elements that are consumed
     *
     * \note Thread-safe and non-blocking, if functor is thread-safe and non-blocking
     * */
    template < typename Functor >
    size_t consume_all_atomic( Functor&& f )
    {
        size_t             element_count = 0;
        tagged_node_handle old_tos       = tos.load( detail::memory_order_acquire );

        for ( ;; ) {
            node* old_tos_pointer = pool.get_pointer( old_tos );
            if ( !old_tos_pointer )
                return 0;

            tagged_node_handle new_tos( pool.null_handle(), old_tos.get_next_tag() );

            if ( tos.compare_exchange_weak( old_tos, new_tos ) )
                break;
        }

        tagged_node_handle nodes_to_consume = old_tos;

        for ( ;; ) {
            node* node_pointer = pool.get_pointer( nodes_to_consume );
            f( std::move( node_pointer->v ) );
            element_count += 1;

            node* next_node = pool.get_pointer( node_pointer->next );

            if ( !next_node ) {
                pool.template destruct< true >( nodes_to_consume );
                break;
            }

            tagged_node_handle next( pool.get_handle( next_node ), nodes_to_consume.get_next_tag() );
            pool.template destruct< true >( nodes_to_consume );
            nodes_to_consume = next;
        }

        return element_count;
    }

    /** consumes all elements via a functor
     *
     * atomically pops all elements from the stack and applies the functor on each object in reversed order
     *
     * \returns number of elements that are consumed
     *
     * \note Thread-safe and non-blocking, if functor is thread-safe and non-blocking
     * */
    template < typename Functor >
    size_t consume_all_atomic_reversed( Functor&& f )
    {
        size_t             element_count = 0;
        tagged_node_handle old_tos       = tos.load( detail::memory_order_acquire );

        for ( ;; ) {
            node* old_tos_pointer = pool.get_pointer( old_tos );
            if ( !old_tos_pointer )
                return 0;

            tagged_node_handle new_tos( pool.null_handle(), old_tos.get_next_tag() );

            if ( tos.compare_exchange_weak( old_tos, new_tos ) )
                break;
        }

        tagged_node_handle nodes_to_consume = old_tos;

        node*              last_node_pointer = NULL;
        tagged_node_handle nodes_in_reversed_order;
        for ( ;; ) {
            node* node_pointer = pool.get_pointer( nodes_to_consume );
            node* next_node    = pool.get_pointer( node_pointer->next );

            node_pointer->next = pool.get_handle( last_node_pointer );
            last_node_pointer  = node_pointer;

            if ( !next_node ) {
                nodes_in_reversed_order = nodes_to_consume;
                break;
            }

            tagged_node_handle next( pool.get_handle( next_node ), nodes_to_consume.get_next_tag() );
            nodes_to_consume = next;
        }

        for ( ;; ) {
            node* node_pointer = pool.get_pointer( nodes_in_reversed_order );
            f( std::move( node_pointer->v ) );
            element_count += 1;

            node* next_node = pool.get_pointer( node_pointer->next );

            if ( !next_node ) {
                pool.template destruct< true >( nodes_in_reversed_order );
                break;
            }

            tagged_node_handle next( pool.get_handle( next_node ), nodes_in_reversed_order.get_next_tag() );
            pool.template destruct< true >( nodes_in_reversed_order );
            nodes_in_reversed_order = next;
        }

        return element_count;
    }
    /**
     * \return true, if stack is empty.
     *
     * \note It only guarantees that at some point during the execution of the function the stack has been empty.
     *       It is rarely practical to use this value in program logic, because the stack can be modified by other threads.
     * */
    bool empty( void ) const
    {
        return pool.get_pointer( tos.load() ) == NULL;
    }

private:
#ifndef BOOST_DOXYGEN_INVOKED
    detail::atomic< tagged_node_handle > tos;

    alignas( detail::cacheline_bytes ) pool_t pool;
#endif
};

}} // namespace boost::lockfree

#endif /* BOOST_LOCKFREE_STACK_HPP_INCLUDED */
