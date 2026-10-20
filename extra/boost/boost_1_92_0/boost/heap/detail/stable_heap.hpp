// boost heap: helper classes for stable priority queues
//
// Copyright (C) 2010 Tim Blechmann
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_HEAP_DETAIL_STABLE_HEAP_HPP
#define BOOST_HEAP_DETAIL_STABLE_HEAP_HPP

#include <limits>
#include <stdexcept>
#include <type_traits>
#include <utility>

#include <boost/core/allocator_access.hpp>
#include <boost/cstdint.hpp>
#include <boost/iterator/iterator_adaptor.hpp>
#include <boost/throw_exception.hpp>

#include <boost/heap/detail/heap_utils.hpp>
#include <boost/heap/heap_merge.hpp>
#include <boost/heap/policies.hpp>

namespace boost { namespace heap { namespace detail {

template < bool ConstantSize, class SizeType >
struct size_holder
{
    static const bool constant_time_size = ConstantSize;
    typedef SizeType  size_type;

    size_holder( void ) noexcept = default;

    size_holder( size_holder&& rhs ) noexcept :
        size_( rhs.size_ )
    {
        rhs.size_ = 0;
    }

    size_holder( size_holder const& rhs ) noexcept = default;

    size_holder& operator=( size_holder&& rhs ) noexcept
    {
        size_     = rhs.size_;
        rhs.size_ = 0;
        return *this;
    }

    size_holder& operator=( size_holder const& rhs ) noexcept = default;

    SizeType get_size() const noexcept
    {
        return size_;
    }

    void set_size( SizeType size ) noexcept
    {
        size_ = size;
    }

    void decrement() noexcept
    {
        --size_;
    }

    void increment() noexcept
    {
        ++size_;
    }

    void add( SizeType value ) noexcept
    {
        size_ += value;
    }

    void sub( SizeType value ) noexcept
    {
        size_ -= value;
    }

    void do_swap( size_holder& rhs ) noexcept
    {
        swap_via_move( *this, rhs );
    }

    SizeType size_ {};
};

template < class SizeType >
struct size_holder< false, SizeType >
{
    static const bool constant_time_size = false;
    typedef SizeType  size_type;

    size_type get_size() const noexcept
    {
        return 0;
    }

    void set_size( size_type ) noexcept
    {}

    void decrement() noexcept
    {}

    void increment() noexcept
    {}

    void add( SizeType /*value*/ ) noexcept
    {}

    void sub( SizeType /*value*/ ) noexcept
    {}

    void do_swap( size_holder& /*rhs*/ ) noexcept
    {}
};

template < typename T, typename Cmp, bool constant_time_size, typename StabilityCounterType = boost::uintmax_t, bool stable = false >
struct heap_base : Cmp, size_holder< constant_time_size, size_t >
{
    typedef StabilityCounterType                      stability_counter_type;
    typedef T                                         value_type;
    typedef T                                         internal_type;
    typedef size_holder< constant_time_size, size_t > size_holder_type;
    typedef Cmp                                       value_compare;
    typedef Cmp                                       internal_compare;
    static const bool                                 is_stable = stable;

    heap_base( Cmp const& cmp = Cmp() ) :
        Cmp( cmp )
    {}

    heap_base( heap_base&& rhs ) noexcept( std::is_nothrow_move_constructible< Cmp >::value ) :
        Cmp( std::move( static_cast< Cmp& >( rhs ) ) ),
        size_holder_type( std::move( static_cast< size_holder_type& >( rhs ) ) )
    {}

    heap_base( heap_base const& rhs ) :
        Cmp( static_cast< Cmp const& >( rhs ) ),
        size_holder_type( static_cast< size_holder_type const& >( rhs ) )
    {}

    heap_base& operator=( heap_base&& rhs ) noexcept( std::is_nothrow_move_assignable< Cmp >::value )
    {
        value_comp_ref().operator=( std::move( rhs.value_comp_ref() ) );
        size_holder_type::operator=( std::move( static_cast< size_holder_type& >( rhs ) ) );
        return *this;
    }

    heap_base& operator=( heap_base const& rhs )
    {
        value_comp_ref().operator=( rhs.value_comp() );
        size_holder_type::operator=( static_cast< size_holder_type const& >( rhs ) );
        return *this;
    }

    bool operator()( internal_type const& lhs, internal_type const& rhs ) const
    {
        return value_comp().operator()( lhs, rhs );
    }

    internal_type make_node( T const& val )
    {
        return val;
    }

    T&& make_node( T&& val )
    {
        return std::forward< T >( val );
    }

    template < class... Args >
    internal_type make_node( Args&&... val )
    {
        return internal_type( std::forward< Args >( val )... );
    }

    static T& get_value( internal_type& val ) noexcept
    {
        return val;
    }

    static T const& get_value( internal_type const& val ) noexcept
    {
        return val;
    }

    Cmp const& value_comp( void ) const noexcept
    {
        return *this;
    }

    Cmp const& get_internal_cmp( void ) const noexcept
    {
        return value_comp();
    }

    void do_swap( heap_base& rhs ) noexcept( std::is_nothrow_move_constructible< Cmp >::value
                                             && std::is_nothrow_move_assignable< Cmp >::value )
    {
        heap_base tmp( std::move( rhs ) );
        rhs   = std::move( *this );
        *this = std::move( tmp );
    }

    stability_counter_type get_stability_count( void ) const noexcept
    {
        return 0;
    }

    void set_stability_count( stability_counter_type ) noexcept
    {}

    template < typename Heap1, typename Heap2 >
    friend struct heap_merge_emulate;

private:
    Cmp& value_comp_ref( void )
    {
        return *this;
    }
};


template < typename T, typename Cmp, bool constant_time_size, typename StabilityCounterType >
struct heap_base< T, Cmp, constant_time_size, StabilityCounterType, true > :
    Cmp,
    size_holder< constant_time_size, size_t >
{
    typedef StabilityCounterType stability_counter_type;
    typedef T                    value_type;

    struct internal_type
    {
        template < class... Args >
        internal_type( stability_counter_type cnt, Args&&... args ) :
            first( std::forward< Args >( args )... ),
            second( cnt )
        {}

        internal_type( stability_counter_type const& cnt, T const& value ) :
            first( value ),
            second( cnt )
        {}

        T                      first;
        stability_counter_type second;
    };

    typedef size_holder< constant_time_size, size_t > size_holder_type;
    typedef Cmp                                       value_compare;

    heap_base( Cmp const& cmp = Cmp() ) :
        Cmp( cmp ),
        counter_( 0 )
    {}

    heap_base( heap_base const& rhs )            = default;
    heap_base& operator=( heap_base const& rhs ) = default;

    heap_base( heap_base&& rhs ) noexcept( std::is_nothrow_move_constructible< Cmp >::value )
    {
        *this = std::move( rhs );
    }

    heap_base& operator=( heap_base&& rhs ) noexcept( std::is_nothrow_move_assignable< Cmp >::value )
    {
        value_comp_ref().operator=( std::move( rhs.value_comp_ref() ) );
        size_holder_type::operator=( std::move( static_cast< size_holder_type& >( rhs ) ) );

        counter_     = rhs.counter_;
        rhs.counter_ = 0;
        return *this;
    }


    bool operator()( internal_type const& lhs, internal_type const& rhs ) const
    {
        return get_internal_cmp()( lhs, rhs );
    }

    bool operator()( T const& lhs, T const& rhs ) const
    {
        return value_comp()( lhs, rhs );
    }

    internal_type make_node( T const& val )
    {
        if ( counter_ == ( std::numeric_limits< stability_counter_type >::max )() - 1 )
            BOOST_THROW_EXCEPTION( std::runtime_error( "boost::heap counter overflow" ) );
        stability_counter_type count = ++counter_;
        return internal_type( count, val );
    }

    template < class... Args >
    internal_type make_node( Args&&... args )
    {
        if ( counter_ == ( std::numeric_limits< stability_counter_type >::max )() - 1 )
            BOOST_THROW_EXCEPTION( std::runtime_error( "boost::heap counter overflow" ) );
        stability_counter_type count = ++counter_;
        return internal_type( count, std::forward< Args >( args )... );
    }

    static T& get_value( internal_type& val ) noexcept
    {
        return val.first;
    }

    static T const& get_value( internal_type const& val ) noexcept
    {
        return val.first;
    }

    Cmp const& value_comp( void ) const noexcept
    {
        return *this;
    }

    struct internal_compare : Cmp
    {
        internal_compare( Cmp const& cmp = Cmp() ) :
            Cmp( cmp )
        {}

        bool operator()( internal_type const& lhs, internal_type const& rhs ) const
        {
            if ( Cmp::operator()( lhs.first, rhs.first ) )
                return true;

            if ( Cmp::operator()( rhs.first, lhs.first ) )
                return false;

            return lhs.second > rhs.second;
        }
    };

    internal_compare get_internal_cmp( void ) const
    {
        return internal_compare( value_comp() );
    }

    void do_swap( heap_base& rhs ) noexcept( std::is_nothrow_move_constructible< Cmp >::value
                                             && std::is_nothrow_move_assignable< Cmp >::value )
    {
        heap_base tmp( std::move( rhs ) );
        rhs   = std::move( *this );
        *this = std::move( tmp );
    }

    stability_counter_type get_stability_count( void ) const
    {
        return counter_;
    }

    void set_stability_count( stability_counter_type new_count )
    {
        counter_ = new_count;
    }

    template < typename Heap1, typename Heap2 >
    friend struct heap_merge_emulate;

private:
    Cmp& value_comp_ref( void ) noexcept
    {
        return *this;
    }

    stability_counter_type counter_;
};

template < typename node_pointer, typename extractor, typename reference >
struct node_handle
{
    explicit node_handle( node_pointer n = 0 ) :
        node_( n )
    {}

    reference operator*() const
    {
        return extractor::get_value( node_->value );
    }

    bool operator==( node_handle const& rhs ) const
    {
        return node_ == rhs.node_;
    }

    bool operator!=( node_handle const& rhs ) const
    {
        return node_ != rhs.node_;
    }

    node_pointer node_;
};

template < typename value_type, typename internal_type, typename extractor >
struct value_extractor
{
    value_type const& operator()( internal_type const& data ) const
    {
        return extractor::get_value( data );
    }
};

template < typename T, typename ContainerIterator, typename Extractor >
class stable_heap_iterator :
    public boost::iterator_adaptor< stable_heap_iterator< T, ContainerIterator, Extractor >,
                                    ContainerIterator,
                                    T const,
                                    boost::random_access_traversal_tag >
{
    typedef boost::iterator_adaptor< stable_heap_iterator, ContainerIterator, T const, boost::random_access_traversal_tag >
        super_t;

public:
    stable_heap_iterator( void ) :
        super_t( 0 )
    {}

    explicit stable_heap_iterator( ContainerIterator const& it ) :
        super_t( it )
    {}

private:
    friend class boost::iterator_core_access;

    T const& dereference() const
    {
        return Extractor::get_value( *super_t::base() );
    }
};

template < typename T, typename Parspec, bool constant_time_size >
struct make_heap_base
{
    typedef typename parameter::binding< Parspec, tag::compare, std::less< T > >::type        compare_argument;
    typedef typename parameter::binding< Parspec, tag::allocator, std::allocator< T > >::type allocator_argument;
    typedef
        typename parameter::binding< Parspec, tag::stability_counter_type, boost::uintmax_t >::type stability_counter_type;

    static const bool is_stable = extract_stable< Parspec >::value;

    typedef heap_base< T, compare_argument, constant_time_size, stability_counter_type, is_stable > type;
};


template < typename Alloc >
struct extract_allocator_types
{
    typedef typename boost::allocator_size_type< Alloc >::type       size_type;
    typedef typename boost::allocator_difference_type< Alloc >::type difference_type;
    typedef typename Alloc::value_type&                              reference;
    typedef typename Alloc::value_type const&                        const_reference;
    typedef typename boost::allocator_pointer< Alloc >::type         pointer;
    typedef typename boost::allocator_const_pointer< Alloc >::type   const_pointer;
};


}}} // namespace boost::heap::detail

#endif /* BOOST_HEAP_DETAIL_STABLE_HEAP_HPP */
