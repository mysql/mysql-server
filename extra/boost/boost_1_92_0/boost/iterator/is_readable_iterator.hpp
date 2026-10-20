// Copyright David Abrahams 2003. Use, modification and distribution is
// subject to the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#ifndef IS_READABLE_ITERATOR_DWA2003112_HPP
#define IS_READABLE_ITERATOR_DWA2003112_HPP

#include <iterator>
#include <type_traits>
#include <boost/config.hpp>

namespace boost {
namespace iterators {
namespace detail {

// Guts of is_readable_iterator.  It is the iterator type and
// Value is the iterator's value_type.
template< typename It, typename Value >
struct is_readable_iterator_impl :
    public std::is_convertible<
        decltype(*std::declval< It& >()),
        typename std::add_lvalue_reference< Value >::type
    >
{
};

//
// void specializations to handle std input and output iterators
//
template< typename It >
struct is_readable_iterator_impl< It, void > :
    public std::false_type
{
};

template< typename It >
struct is_readable_iterator_impl< It, const void > :
    public std::false_type
{
};

template< typename It >
struct is_readable_iterator_impl< It, volatile void > :
    public std::false_type
{
};

template< typename It >
struct is_readable_iterator_impl< It, const volatile void > :
    public std::false_type
{
};

} // namespace detail

template< typename T >
struct is_readable_iterator :
    public iterators::detail::is_readable_iterator_impl<
        T,
        typename std::iterator_traits< T >::value_type const
    >::type
{
};

#if !defined(BOOST_NO_CXX14_VARIABLE_TEMPLATES)
template< typename T >
BOOST_INLINE_VARIABLE constexpr bool is_readable_iterator_v = iterators::is_readable_iterator< T >::value;
#endif // !defined(BOOST_NO_CXX14_VARIABLE_TEMPLATES)

} // namespace iterators

using iterators::is_readable_iterator;

#if !defined(BOOST_NO_CXX14_VARIABLE_TEMPLATES)
using iterators::is_readable_iterator_v;
#endif // !defined(BOOST_NO_CXX14_VARIABLE_TEMPLATES)

} // namespace boost

#endif // IS_READABLE_ITERATOR_DWA2003112_HPP
