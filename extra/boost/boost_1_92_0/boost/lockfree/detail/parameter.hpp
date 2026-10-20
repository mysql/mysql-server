// boost lockfree
//
// Copyright (C) 2011, 2016 Tim Blechmann
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_LOCKFREE_DETAIL_PARAMETER_HPP
#define BOOST_LOCKFREE_DETAIL_PARAMETER_HPP

#include <boost/align/aligned_allocator.hpp>
#include <boost/core/allocator_access.hpp>
#include <boost/lockfree/detail/prefix.hpp>
#include <boost/lockfree/policies.hpp>
#include <boost/parameter/binding.hpp>

#include <type_traits>

namespace boost { namespace lockfree { namespace detail {

//----------------------------------------------------------------------------------------------------------------------

template < typename bound_args, typename tag_type, typename default_ >
using extract_arg_or_default_t = typename parameter::binding< bound_args, tag_type, default_ >::type;


template < typename BoundArgs, typename TypeTag, typename IntegralType, IntegralType default_ = IntegralType {} >
struct extract_integral_arg_or_default_t
{
    static constexpr IntegralType value
        = extract_arg_or_default_t< BoundArgs, TypeTag, std::integral_constant< IntegralType, default_ > >::value;
};


struct no_such_parameter_t
{};

template < typename bound_args, typename tag_type >
using has_no_arg_t
    = std::is_same< extract_arg_or_default_t< bound_args, tag_type, no_such_parameter_t >, no_such_parameter_t >;

//----------------------------------------------------------------------------------------------------------------------

template < typename bound_args >
struct extract_capacity
{
    using capacity_t = extract_arg_or_default_t< bound_args, tag::capacity, std::integral_constant< size_t, 0 > >;
    using has_no_capacity_t                   = has_no_arg_t< bound_args, tag::capacity >;
    static constexpr std::size_t capacity     = capacity_t::value;
    static constexpr bool        has_capacity = !has_no_capacity_t::value;
};

template < typename bound_args >
using extract_capacity_t = typename extract_capacity< bound_args >::type;

//----------------------------------------------------------------------------------------------------------------------

template < typename bound_args, typename T >
struct extract_allocator
{
    using default_allocator = boost::alignment::aligned_allocator< T, cacheline_bytes >;
    using allocator_t       = extract_arg_or_default_t< bound_args, tag::allocator, default_allocator >;

    using has_no_allocator_t            = has_no_arg_t< bound_args, tag::allocator >;
    static constexpr bool has_allocator = !has_no_allocator_t::value;

    typedef typename boost::allocator_rebind< allocator_t, T >::type type;
};

template < typename bound_args, typename T >
using extract_allocator_t = typename extract_allocator< bound_args, T >::type;

//----------------------------------------------------------------------------------------------------------------------

template < typename bound_args, bool default_ = false >
using extract_fixed_sized = extract_integral_arg_or_default_t< bound_args, tag::fixed_sized, bool, default_ >;

//----------------------------------------------------------------------------------------------------------------------

template < typename bound_args, bool default_ = false >
using extract_allow_multiple_reads
    = extract_integral_arg_or_default_t< bound_args, tag::allow_multiple_reads, bool, default_ >;

template < typename bound_args, bool default_ = false >
using extract_freelist = extract_integral_arg_or_default_t< bound_args, tag::freelist, bool, default_ >;

template < typename bound_args, bool default_ = false >
using extract_single_producer = extract_integral_arg_or_default_t< bound_args, tag::single_producer, bool, default_ >;

template < typename bound_args, bool default_ = false >
using extract_single_consumer = extract_integral_arg_or_default_t< bound_args, tag::single_consumer, bool, default_ >;

//----------------------------------------------------------------------------------------------------------------------

}}} // namespace boost::lockfree::detail

#endif /* BOOST_LOCKFREE_DETAIL_PARAMETER_HPP */
