//  Copyright (C) 2008-2016 Tim Blechmann
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)


#ifndef BOOST_LOCKFREE_FORWARD_HPP_INCLUDED
#define BOOST_LOCKFREE_FORWARD_HPP_INCLUDED

#include <boost/config.hpp>
#ifdef BOOST_HAS_PRAGMA_ONCE
#    pragma once
#endif


#ifndef BOOST_DOXYGEN_INVOKED

#    include <cstddef>
#    if !defined( BOOST_NO_CXX20_HDR_CONCEPTS )
#        include <type_traits>
#    endif

namespace boost { namespace lockfree {

// policies
template < bool IsFixedSized >
struct fixed_sized;

template < size_t Size >
struct capacity;

template < class Alloc >
struct allocator;


// data structures

template < typename T, typename... Options >
#    if !defined( BOOST_NO_CXX20_HDR_CONCEPTS )
    requires( std::is_copy_assignable_v< T >,
              std::is_trivially_copy_assignable_v< T >,
              std::is_trivially_destructible_v< T > )
#    endif
class queue;

template < typename T, typename... Options >
#    if !defined( BOOST_NO_CXX20_HDR_CONCEPTS )
    requires( std::is_copy_assignable_v< T > || std::is_move_assignable_v< T > )
#    endif
class stack;

template < typename T, typename... Options >
#    if !defined( BOOST_NO_CXX20_HDR_CONCEPTS )
    requires( std::is_default_constructible_v< T >, std::is_move_assignable_v< T > || std::is_copy_assignable_v< T > )
#    endif
class spsc_queue;

template < typename T, typename... Options >
struct spsc_value;

template < typename T, typename... Options >
class bounded_ticket_queue;

}} // namespace boost::lockfree

#endif // BOOST_DOXYGEN_INVOKED
#endif // BOOST_LOCKFREE_FORWARD_HPP_INCLUDED
