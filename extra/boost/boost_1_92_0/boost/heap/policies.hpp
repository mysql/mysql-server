// boost heap
//
// Copyright (C) 2010-2011 Tim Blechmann
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_HEAP_POLICIES_HPP
#define BOOST_HEAP_POLICIES_HPP

#include <boost/concept_check.hpp>
#include <boost/parameter/aux_/void.hpp>
#include <boost/parameter/binding.hpp>
#include <boost/parameter/name.hpp>
#include <boost/parameter/parameters.hpp>
#include <boost/parameter/template_keyword.hpp>

#include <type_traits>

#ifdef BOOST_HAS_PRAGMA_ONCE
#    pragma once
#endif

namespace boost { namespace heap {

#ifndef BOOST_DOXYGEN_INVOKED
BOOST_PARAMETER_TEMPLATE_KEYWORD( allocator )
BOOST_PARAMETER_TEMPLATE_KEYWORD( compare )

namespace tag {
struct stable;
} // namespace tag

template < bool T >
struct stable : boost::parameter::template_keyword< tag::stable, std::integral_constant< bool, T > >
{};

namespace tag {
struct mutable_;
} // namespace tag

template < bool T >
struct mutable_ : boost::parameter::template_keyword< tag::mutable_, std::integral_constant< bool, T > >
{};


namespace tag {
struct constant_time_size;
} // namespace tag

template < bool T >
struct constant_time_size :
    boost::parameter::template_keyword< tag::constant_time_size, std::integral_constant< bool, T > >
{};

namespace tag {
struct store_parent_pointer;
} // namespace tag

template < bool T >
struct store_parent_pointer :
    boost::parameter::template_keyword< tag::store_parent_pointer, std::integral_constant< bool, T > >
{};

namespace tag {
struct arity;
} // namespace tag

template < unsigned int T >
struct arity : boost::parameter::template_keyword< tag::arity, std::integral_constant< int, T > >
{};

namespace tag {
struct objects_per_page;
} // namespace tag

template < unsigned int T >
struct objects_per_page : boost::parameter::template_keyword< tag::objects_per_page, std::integral_constant< int, T > >
{};

BOOST_PARAMETER_TEMPLATE_KEYWORD( stability_counter_type )

namespace detail {

template < typename bound_args, typename tag_type >
struct has_arg
{
    typedef typename boost::parameter::binding< bound_args, tag_type, void >::type type;
    static const bool                                                              value = !std::is_void< type >::value;
};

template < typename bound_args >
struct extract_stable
{
    static const bool has_stable = has_arg< bound_args, tag::stable >::value;

    typedef
        typename std::conditional< has_stable, typename has_arg< bound_args, tag::stable >::type, std::false_type >::type
            stable_t;

    static const bool value = stable_t::value;
};

template < typename bound_args >
struct extract_mutable
{
    static const bool has_mutable = has_arg< bound_args, tag::mutable_ >::value;

    typedef
        typename std::conditional< has_mutable, typename has_arg< bound_args, tag::mutable_ >::type, std::false_type >::type
            mutable_t;

    static const bool value = mutable_t::value;
};

} // namespace detail

#else

/** \brief Specifies the predicate for the heap order
 */
template < typename T >
struct compare
{};

/** \brief Configure heap as mutable
 *
 *  Certain heaps need to be configured specifically do be mutable.
 *
 * */
template < bool T >
struct mutable_
{};

/** \brief Specifies allocator for the internal memory management
 */
template < typename T >
struct allocator
{};

/** \brief Configure a heap as \b stable
 *
 * A priority queue is stable, if elements with the same priority are popped from the heap, in the same order as
 * they are inserted.
 * */
template < bool T >
struct stable
{};

/** \brief Specifies the type for stability counter
 *
 * */
template < typename IntType >
struct stability_counter_type
{};

/** \brief Configures complexity of <tt> size() </tt>
 *
 * Specifies, whether size() should have linear or constant complexity.
 * */
template < bool T >
struct constant_time_size
{};

/** \brief Store parent pointer in heap node.
 *
 * Maintaining a parent pointer adds some maintenance and size overhead, but iterating a heap is more efficient.
 * */
template < bool T >
struct store_parent_pointer
{};

/** \brief Specify arity.
 *
 * Specifies the arity of a D-ary heap
 * */
template < unsigned int T >
struct arity
{};
#endif

}} // namespace boost::heap

#endif /* BOOST_HEAP_POLICIES_HPP */
