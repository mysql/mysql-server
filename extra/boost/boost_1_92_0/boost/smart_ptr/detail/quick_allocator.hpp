#ifndef BOOST_SMART_PTR_DETAIL_QUICK_ALLOCATOR_HPP_INCLUDED
#define BOOST_SMART_PTR_DETAIL_QUICK_ALLOCATOR_HPP_INCLUDED

// Copyright 2003 David Abrahams
// Copyright 2003, 2025 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/config/header_deprecated.hpp>
#include <memory>
#include <cstddef>

BOOST_HEADER_DEPRECATED("std::allocator or std::pmr::synchronized_pool_resource")

namespace boost
{
namespace detail
{

template<class T> struct quick_allocator
{
    static void* alloc()
    {
        return std::allocator<T>().allocate( 1 );
    }

    static void* alloc( std::size_t n )
    {
        if( n != sizeof(T) ) // class-specific delete called for a derived object
        {
            return ::operator new( n );
        }
        else
        {
            return alloc();
        }
    }

    static void dealloc( void* p )
    {
        if( p != 0 ) // 18.4.1.1/13
        {
            std::allocator<T>().deallocate( static_cast<T*>( p ), 1 );
        }
    }

    static void dealloc( void* p, std::size_t n )
    {
        if( n != sizeof(T) ) // class-specific delete called for a derived object
        {
            ::operator delete( p );
        }
        else
        {
            dealloc( p );
        }
    }
};

} // namespace detail
} // namespace boost

#endif  // #ifndef BOOST_SMART_PTR_DETAIL_QUICK_ALLOCATOR_HPP_INCLUDED
