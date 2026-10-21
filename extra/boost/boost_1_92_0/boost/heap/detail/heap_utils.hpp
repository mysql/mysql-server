// boost heap: heap utilities
//
// Copyright (C) 2026 Tim Blechmann
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_HEAP_DETAIL_HEAP_UTILS_HPP
#define BOOST_HEAP_DETAIL_HEAP_UTILS_HPP

#include <type_traits>
#include <utility>

namespace boost { namespace heap { namespace detail {


template < typename T >
inline void swap_via_move( T& lhs, T& rhs ) noexcept( std::is_nothrow_move_constructible< T >::value
                                                      && std::is_nothrow_move_assignable< T >::value )
{
    T tmp( std::move( lhs ) );
    lhs = std::move( rhs );
    rhs = std::move( tmp );
}

}}} // namespace boost::heap::detail

#endif /* BOOST_HEAP_DETAIL_HEAP_UTILS_HPP */
