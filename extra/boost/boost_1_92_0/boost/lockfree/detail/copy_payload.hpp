//  boost lockfree: copy_payload helper
//
//  Copyright (C) 2011, 2016 Tim Blechmann
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_LOCKFREE_DETAIL_COPY_PAYLOAD_HPP_INCLUDED
#define BOOST_LOCKFREE_DETAIL_COPY_PAYLOAD_HPP_INCLUDED

#include <type_traits>

#if defined( _MSC_VER )
#    pragma warning( push )
#    pragma warning( disable : 4512 ) // assignment operator could not be generated
#endif

namespace boost { namespace lockfree { namespace detail {

struct copy_convertible
{
    template < typename T, typename U >
    static void copy( T& t, U& u )
    {
        u = t;
    }
};

struct copy_constructible_and_copyable
{
    template < typename T, typename U >
    static void copy( T& t, U& u )
    {
        u = U( t );
    }
};

template < typename T, typename U >
void copy_payload( T& t, U& u )
{
    static constexpr bool is_convertible = std::is_convertible< T, U >::value;
    typedef std::conditional_t< is_convertible, copy_convertible, copy_constructible_and_copyable > copy_type;
    copy_type::copy( t, u );
}

}}} // namespace boost::lockfree::detail

#if defined( _MSC_VER )
#    pragma warning( pop )
#endif

#endif /* BOOST_LOCKFREE_DETAIL_COPY_PAYLOAD_HPP_INCLUDED */
