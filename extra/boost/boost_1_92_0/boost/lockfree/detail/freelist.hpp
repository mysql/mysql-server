//  lock-free freelist
//
//  Copyright (C) 2008-2016 Tim Blechmann
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_LOCKFREE_FREELIST_HPP_INCLUDED
#define BOOST_LOCKFREE_FREELIST_HPP_INCLUDED

#include <array>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <stdexcept>

#include <boost/align/align_up.hpp>
#if __cplusplus < 201703L
#    include <boost/align/aligned_allocator_adaptor.hpp>
#endif
#include <boost/config.hpp>
#include <boost/static_assert.hpp>
#include <boost/throw_exception.hpp>

#include <boost/lockfree/detail/atomic.hpp>
#include <boost/lockfree/detail/parameter.hpp>
#include <boost/lockfree/detail/tagged_ptr.hpp>

#if defined( _MSC_VER )
#    pragma warning( push )
#    pragma warning( disable : 4100 ) // unreferenced formal parameter
#    pragma warning( disable : 4127 ) // conditional expression is constant
#endif

namespace boost { namespace lockfree { namespace detail {

//----------------------------------------------------------------------------------------------------------------------

template < typename Alloc, std::size_t Align = cacheline_bytes >
struct select_allocator
{
#if __cplusplus >= 201703L
    typedef Alloc type;
#else
    typedef boost::alignment::aligned_allocator_adaptor< Alloc, Align > type;
#endif
};

template < typename Alloc, std::size_t Align = cacheline_bytes >
using select_allocator_t = typename select_allocator< Alloc, Align >::type;

//----------------------------------------------------------------------------------------------------------------------

template < typename T, typename Alloc = std::allocator< T > >
class alignas( cacheline_bytes ) freelist_stack : select_allocator_t< Alloc >
{
    typedef select_allocator_t< Alloc > allocator_type;

    struct BOOST_MAY_ALIAS freelist_node
    {
#if __cplusplus >= 201703L
        static constexpr std::size_t freelist_node_alignment = alignof( T );
#else
        static constexpr std::size_t freelist_node_alignment = alignof( std::max_align_t );
#endif
        static constexpr auto tagged_ptr_alignment = alignof( tagged_ptr< freelist_node > );
        static constexpr auto member_alignment     = std::max( freelist_node_alignment, tagged_ptr_alignment );

        alignas( member_alignment ) tagged_ptr< freelist_node > next;
    };

    typedef tagged_ptr< freelist_node > tagged_node_ptr;

public:
    typedef T*              index_t;
    typedef tagged_ptr< T > tagged_node_handle;

    template < typename Allocator >
    freelist_stack( Allocator const& alloc, std::size_t n = 0 ) :
        allocator_type( alloc ),
        pool_( tagged_node_ptr( NULL ) )
    {
        for ( std::size_t i = 0; i != n; ++i ) {
            T* node = allocator_type::allocate( 1 );
            std::memset( (void*)node, 0, sizeof( T ) );
#ifdef BOOST_LOCKFREE_FREELIST_INIT_RUNS_DTOR
            destruct< false >( node );
#else
            deallocate< false >( node );
#endif
        }
    }

    template < bool ThreadSafe >
    void reserve( std::size_t count )
    {
        for ( std::size_t i = 0; i != count; ++i ) {
            T* node = allocator_type::allocate( 1 );
            std::memset( (void*)node, 0, sizeof( T ) );
            deallocate< ThreadSafe >( node );
        }
    }

    template < bool ThreadSafe, bool Bounded >
    T* construct( void )
    {
        T* node = allocate< ThreadSafe, Bounded >();
        if ( node ) {
            freelist_node*  fn  = reinterpret_cast< freelist_node* >( node );
            tagged_node_ptr old = fn->next;
            new ( node ) T( old.get_next_tag() );
        }
        return node;
    }

    template < bool ThreadSafe, bool Bounded, typename ArgumentType >
    T* construct( ArgumentType&& arg )
    {
        T* node = allocate< ThreadSafe, Bounded >();
        if ( node ) {
            freelist_node*  fn  = reinterpret_cast< freelist_node* >( node );
            tagged_node_ptr old = fn->next;
            new ( node ) T( std::forward< ArgumentType >( arg ), old.get_next_tag() );
        }
        return node;
    }

    template < bool ThreadSafe, bool Bounded, typename ArgumentType1, typename ArgumentType2 >
    T* construct( ArgumentType1&& arg1, ArgumentType2&& arg2 )
    {
        T* node = allocate< ThreadSafe, Bounded >();
        if ( node ) {
            freelist_node*  fn  = reinterpret_cast< freelist_node* >( node );
            tagged_node_ptr old = fn->next;
            new ( node )
                T( std::forward< ArgumentType1 >( arg1 ), std::forward< ArgumentType2 >( arg2 ), old.get_next_tag() );
        }
        return node;
    }

    template < bool ThreadSafe >
    void destruct( tagged_node_handle const& tagged_ptr )
    {
        T* n = tagged_ptr.get_ptr();
        n->~T();
        deallocate< ThreadSafe >( n );
    }

    template < bool ThreadSafe >
    void destruct( T* n )
    {
        n->~T();
        deallocate< ThreadSafe >( n );
    }

    ~freelist_stack( void )
    {
        tagged_node_ptr current = pool_.load();

        while ( current ) {
            freelist_node* current_ptr = current.get_ptr();
            if ( current_ptr )
                current = current_ptr->next;
            allocator_type::deallocate( (T*)current_ptr, 1 );
        }
    }

    bool is_lock_free( void ) const
    {
        return pool_.is_lock_free();
    }

    T* get_handle( T* pointer ) const
    {
        return pointer;
    }

    T* get_handle( tagged_node_handle const& handle ) const
    {
        return get_pointer( handle );
    }

    T* get_pointer( tagged_node_handle const& tptr ) const
    {
        return tptr.get_ptr();
    }

    T* get_pointer( T* pointer ) const
    {
        return pointer;
    }

    T* null_handle( void ) const
    {
        return NULL;
    }

protected: // allow use from subclasses
    template < bool ThreadSafe, bool Bounded >
    T* allocate( void )
    {
        if ( ThreadSafe )
            return allocate_impl< Bounded >();
        else
            return allocate_impl_unsafe< Bounded >();
    }

private:
    template < bool Bounded >
    T* allocate_impl( void )
    {
        tagged_node_ptr old_pool = pool_.load( memory_order_acquire );

        for ( ;; ) {
            if ( !old_pool.get_ptr() ) {
                if ( !Bounded ) {
                    T* ptr = allocator_type::allocate( 1 );
                    std::memset( (void*)ptr, 0, sizeof( T ) );
                    return ptr;
                } else
                    return 0;
            }

            freelist_node*  new_pool_ptr = old_pool->next.get_ptr();
            tagged_node_ptr new_pool( new_pool_ptr, old_pool.get_next_tag() );

            if ( pool_.compare_exchange_weak( old_pool, new_pool ) ) {
                void* ptr = old_pool.get_ptr();
                return reinterpret_cast< T* >( ptr );
            }
        }
    }

    template < bool Bounded >
    T* allocate_impl_unsafe( void )
    {
        tagged_node_ptr old_pool = pool_.load( memory_order_relaxed );

        if ( !old_pool.get_ptr() ) {
            if ( !Bounded ) {
                T* ptr = allocator_type::allocate( 1 );
                std::memset( (void*)ptr, 0, sizeof( T ) );
                return ptr;
            } else
                return 0;
        }

        freelist_node*  new_pool_ptr = old_pool->next.get_ptr();
        tagged_node_ptr new_pool( new_pool_ptr, old_pool.get_next_tag() );

        pool_.store( new_pool, memory_order_relaxed );
        void* ptr = old_pool.get_ptr();
        return reinterpret_cast< T* >( ptr );
    }

protected:
    template < bool ThreadSafe >
    void deallocate( T* n )
    {
        if ( ThreadSafe )
            deallocate_impl( n );
        else
            deallocate_impl_unsafe( n );
    }

private:
    void deallocate_impl( T* n )
    {
        void*           node         = n;
        tagged_node_ptr old_pool     = pool_.load( memory_order_acquire );
        freelist_node*  new_pool_ptr = reinterpret_cast< freelist_node* >( node );

        for ( ;; ) {
            tagged_node_ptr new_pool( new_pool_ptr, old_pool.get_next_tag() );
            new_pool->next.set_ptr( old_pool.get_ptr() );

            if ( pool_.compare_exchange_weak( old_pool, new_pool ) )
                return;
        }
    }

    void deallocate_impl_unsafe( T* n )
    {
        void*           node         = n;
        tagged_node_ptr old_pool     = pool_.load( memory_order_relaxed );
        freelist_node*  new_pool_ptr = reinterpret_cast< freelist_node* >( node );

        tagged_node_ptr new_pool( new_pool_ptr, old_pool.get_next_tag() );
        new_pool->next.set_ptr( old_pool.get_ptr() );

        pool_.store( new_pool, memory_order_relaxed );
    }

    atomic< tagged_node_ptr > pool_;
};

class tagged_index
{
public:
    typedef std::uint16_t tag_t;
    typedef std::uint16_t index_t;

    /** uninitialized constructor */
    tagged_index( void ) noexcept //: index(0), tag(0)
    {}

    /** copy constructor */
    tagged_index( tagged_index const& rhs ) = default;

    explicit tagged_index( index_t i, tag_t t = 0 ) :
        index( i ),
        tag( t )
    {}

    /** index access */
    /* @{ */
    index_t get_index() const
    {
        return index;
    }

    void set_index( index_t i )
    {
        index = i;
    }
    /* @} */

    /** tag access */
    /* @{ */
    tag_t get_tag() const
    {
        return tag;
    }

    tag_t get_next_tag() const
    {
        tag_t next = ( get_tag() + 1u ) & ( std::numeric_limits< tag_t >::max )();
        return next;
    }

    void set_tag( tag_t t )
    {
        tag = t;
    }
    /* @} */

    bool operator==( tagged_index const& rhs ) const
    {
        return ( index == rhs.index ) && ( tag == rhs.tag );
    }

    bool operator!=( tagged_index const& rhs ) const
    {
        return !operator==( rhs );
    }

protected:
    index_t index;
    tag_t   tag;
};

//----------------------------------------------------------------------------------------------------------------------

template < typename T, std::size_t size >
struct alignas( cacheline_bytes ) compiletime_sized_freelist_storage
{
    // array-based freelists only support a 16bit address space.
    BOOST_STATIC_ASSERT( size < 65536 );

    std::array< char, size * sizeof( T ) + 64 > data;

    // unused ... only for API purposes
    template < typename Allocator >
    compiletime_sized_freelist_storage( Allocator const& /* alloc */, std::size_t /* count */ )
    {
        data.fill( 0 );
    }

    T* nodes( void ) const
    {
        char* data_pointer = const_cast< char* >( data.data() );
        return reinterpret_cast< T* >( boost::alignment::align_up( data_pointer, cacheline_bytes ) );
    }

    std::size_t node_count( void ) const
    {
        return size;
    }
};

//----------------------------------------------------------------------------------------------------------------------

template < typename T, typename Alloc = std::allocator< T > >
struct runtime_sized_freelist_storage : select_allocator_t< Alloc >
{
    typedef select_allocator_t< Alloc > allocator_type;
    T*                                  nodes_;
    std::size_t                         node_count_;

    template < typename Allocator >
    runtime_sized_freelist_storage( Allocator const& alloc, std::size_t count ) :
        allocator_type( alloc ),
        node_count_( count )
    {
        if ( count > 65535 )
            boost::throw_exception( std::runtime_error( "boost.lockfree: freelist size is limited to a maximum of "
                                                        "65535 objects" ) );
        nodes_ = allocator_type::allocate( count );
        std::memset( (void*)nodes_, 0, sizeof( T ) * count );
    }

    ~runtime_sized_freelist_storage( void )
    {
        allocator_type::deallocate( nodes_, node_count_ );
    }

    T* nodes( void ) const
    {
        return nodes_;
    }

    std::size_t node_count( void ) const
    {
        return node_count_;
    }
};

//----------------------------------------------------------------------------------------------------------------------

template < typename T, typename NodeStorage = runtime_sized_freelist_storage< T > >
class fixed_size_freelist : NodeStorage
{
    struct BOOST_MAY_ALIAS freelist_node
    {
        alignas( T ) tagged_index next;
    };

    void initialize( void )
    {
        T* nodes = NodeStorage::nodes();
        for ( std::size_t i = 0; i != NodeStorage::node_count(); ++i ) {
            tagged_index* next_index = reinterpret_cast< tagged_index* >( nodes + i );
            next_index->set_index( null_handle() );

#ifdef BOOST_LOCKFREE_FREELIST_INIT_RUNS_DTOR
            destruct< false >( nodes + i );
#else
            deallocate< false >( static_cast< index_t >( i ) );
#endif
        }
    }

public:
    typedef tagged_index          tagged_node_handle;
    typedef tagged_index::index_t index_t;

    template < typename Allocator >
    fixed_size_freelist( Allocator const& alloc, std::size_t count ) :
        NodeStorage( alloc, count ),
        pool_( tagged_index( static_cast< index_t >( count ), 0 ) )
    {
        initialize();
    }

    fixed_size_freelist( void ) :
        pool_( tagged_index( NodeStorage::node_count(), 0 ) )
    {
        initialize();
    }

    template < bool ThreadSafe, bool Bounded >
    T* construct( void )
    {
        index_t node_index = allocate< ThreadSafe >();
        if ( node_index == null_handle() )
            return NULL;

        T*             node = NodeStorage::nodes() + node_index;
        freelist_node* fn   = reinterpret_cast< freelist_node* >( node );
        new ( node ) T( fn->next.get_next_tag() );
        return node;
    }

    template < bool ThreadSafe, bool Bounded, typename ArgumentType >
    T* construct( ArgumentType&& arg )
    {
        index_t node_index = allocate< ThreadSafe >();
        if ( node_index == null_handle() )
            return NULL;

        T*             node = NodeStorage::nodes() + node_index;
        freelist_node* fn   = reinterpret_cast< freelist_node* >( node );
        new ( node ) T( std::forward< ArgumentType >( arg ), fn->next.get_next_tag() );
        return node;
    }

    template < bool ThreadSafe, bool Bounded, typename ArgumentType1, typename ArgumentType2 >
    T* construct( ArgumentType1&& arg1, ArgumentType2&& arg2 )
    {
        index_t node_index = allocate< ThreadSafe >();
        if ( node_index == null_handle() )
            return NULL;

        T*             node = NodeStorage::nodes() + node_index;
        freelist_node* fn   = reinterpret_cast< freelist_node* >( node );
        new ( node )
            T( std::forward< ArgumentType1 >( arg1 ), std::forward< ArgumentType2 >( arg2 ), fn->next.get_next_tag() );
        return node;
    }

    template < bool ThreadSafe >
    void destruct( tagged_node_handle tagged_index )
    {
        index_t index = tagged_index.get_index();
        T*      n     = NodeStorage::nodes() + index;
        (void)n; // silence msvc warning
        n->~T();
        deallocate< ThreadSafe >( index );
    }

    template < bool ThreadSafe >
    void destruct( index_t index )
    {
        T* n = NodeStorage::nodes() + index;
        n->~T();
        deallocate< ThreadSafe >( index );
    }

    template < bool ThreadSafe >
    void destruct( T* n )
    {
        n->~T();
        deallocate< ThreadSafe >( static_cast< index_t >( n - NodeStorage::nodes() ) );
    }

    bool is_lock_free( void ) const
    {
        return pool_.is_lock_free();
    }

    index_t null_handle( void ) const
    {
        return static_cast< index_t >( NodeStorage::node_count() );
    }

    index_t get_handle( T* pointer ) const
    {
        if ( pointer == NULL )
            return null_handle();
        else
            return static_cast< index_t >( pointer - NodeStorage::nodes() );
    }

    index_t get_handle( tagged_node_handle const& handle ) const
    {
        return handle.get_index();
    }

    T* get_pointer( tagged_node_handle const& tptr ) const
    {
        return get_pointer( tptr.get_index() );
    }

    T* get_pointer( index_t index ) const
    {
        if ( index == null_handle() )
            return 0;
        else
            return NodeStorage::nodes() + index;
    }

    T* get_pointer( T* ptr ) const
    {
        return ptr;
    }

protected: // allow use from subclasses
    template < bool ThreadSafe >
    index_t allocate( void )
    {
        if ( ThreadSafe )
            return allocate_impl();
        else
            return allocate_impl_unsafe();
    }

private:
    index_t allocate_impl( void )
    {
        tagged_index old_pool = pool_.load( memory_order_acquire );

        for ( ;; ) {
            index_t index = old_pool.get_index();
            if ( index == null_handle() )
                return index;

            T*            old_node   = NodeStorage::nodes() + index;
            tagged_index* next_index = reinterpret_cast< tagged_index* >( old_node );

            tagged_index new_pool( next_index->get_index(), old_pool.get_next_tag() );

            if ( pool_.compare_exchange_weak( old_pool, new_pool ) )
                return old_pool.get_index();
        }
    }

    index_t allocate_impl_unsafe( void )
    {
        tagged_index old_pool = pool_.load( memory_order_acquire );

        index_t index = old_pool.get_index();
        if ( index == null_handle() )
            return index;

        T*            old_node   = NodeStorage::nodes() + index;
        tagged_index* next_index = reinterpret_cast< tagged_index* >( old_node );

        tagged_index new_pool( next_index->get_index(), old_pool.get_next_tag() );

        pool_.store( new_pool, memory_order_relaxed );
        return old_pool.get_index();
    }

    template < bool ThreadSafe >
    void deallocate( index_t index )
    {
        if ( ThreadSafe )
            deallocate_impl( index );
        else
            deallocate_impl_unsafe( index );
    }

    void deallocate_impl( index_t index )
    {
        freelist_node* new_pool_node = reinterpret_cast< freelist_node* >( NodeStorage::nodes() + index );
        tagged_index   old_pool      = pool_.load( memory_order_acquire );

        for ( ;; ) {
            tagged_index new_pool( index, old_pool.get_next_tag() );
            new_pool_node->next.set_index( old_pool.get_index() );

            if ( pool_.compare_exchange_weak( old_pool, new_pool ) )
                return;
        }
    }

    void deallocate_impl_unsafe( index_t index )
    {
        freelist_node* new_pool_node = reinterpret_cast< freelist_node* >( NodeStorage::nodes() + index );
        tagged_index   old_pool      = pool_.load( memory_order_acquire );

        tagged_index new_pool( index, old_pool.get_next_tag() );
        new_pool_node->next.set_index( old_pool.get_index() );

        pool_.store( new_pool );
    }

    atomic< tagged_index > pool_;
};

//----------------------------------------------------------------------------------------------------------------------

template < typename T, typename Alloc, bool IsCompileTimeSized, bool IsFixedSize, std::size_t Capacity >
struct select_freelist
{
    typedef std::conditional_t< IsCompileTimeSized,
                                compiletime_sized_freelist_storage< T, Capacity >,
                                runtime_sized_freelist_storage< T, Alloc > >
        fixed_sized_storage_type;

    typedef std::conditional_t< IsCompileTimeSized || IsFixedSize,
                                fixed_size_freelist< T, fixed_sized_storage_type >,
                                freelist_stack< T, Alloc > >
        type;
};

template < typename T, typename Alloc, bool IsCompileTimeSized, bool IsFixedSize, std::size_t Capacity >
using select_freelist_t = typename select_freelist< T, Alloc, IsCompileTimeSized, IsFixedSize, Capacity >::type;

//----------------------------------------------------------------------------------------------------------------------

template < typename T, typename Alloc = std::allocator< T > >
class alignas( cacheline_bytes ) direct_allocator : Alloc
{
public:
    typedef T* index_t;
    typedef T* tagged_node_handle;

    template < typename Allocator >
    direct_allocator( Allocator const& alloc, std::size_t = 0 ) :
        Alloc( alloc )
    {}

    template < bool ThreadSafe >
    void reserve( std::size_t )
    {}

    template < bool ThreadSafe, bool Bounded >
    T* construct( void )
    {
        T* node = Alloc::allocate( 1 );
        if ( node )
            new ( node ) T();
        return node;
    }

    template < bool ThreadSafe, bool Bounded, typename ArgumentType >
    T* construct( ArgumentType&& arg )
    {
        T* node = Alloc::allocate( 1 );
        if ( node )
            new ( node ) T( std::forward< ArgumentType >( arg ) );
        return node;
    }

    template < bool ThreadSafe, bool Bounded, typename ArgumentType1, typename ArgumentType2 >
    T* construct( ArgumentType1&& arg1, ArgumentType2&& arg2 )
    {
        T* node = Alloc::allocate( 1 );
        if ( node )
            new ( node ) T( std::forward< ArgumentType1 >( arg1 ), std::forward< ArgumentType2 >( arg2 ) );
        return node;
    }

    template < bool ThreadSafe >
    void destruct( tagged_node_handle node )
    {
        if ( node ) {
            node->~T();
            Alloc::deallocate( node, 1 );
        }
    }

    bool is_lock_free( void ) const
    {
        return false;
    }

    T* get_handle( T* pointer ) const
    {
        return pointer;
    }

    T* get_pointer( T* tptr ) const
    {
        return tptr;
    }

    T* null_handle( void ) const
    {
        return nullptr;
    }
};

//----------------------------------------------------------------------------------------------------------------------

template < typename T, bool IsNodeBased >
struct select_tagged_handle
{
    typedef std::conditional_t< IsNodeBased, tagged_ptr< T >, tagged_index >      tagged_handle_type;
    typedef std::conditional_t< IsNodeBased, T*, typename tagged_index::index_t > handle_type;
};

//----------------------------------------------------------------------------------------------------------------------

}}} // namespace boost::lockfree::detail

#if defined( _MSC_VER )
#    pragma warning( pop )
#endif


#endif /* BOOST_LOCKFREE_FREELIST_HPP_INCLUDED */
