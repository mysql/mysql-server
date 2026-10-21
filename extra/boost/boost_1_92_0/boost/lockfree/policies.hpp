// boost lockfree
//
// Copyright (C) 2011 Tim Blechmann
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_LOCKFREE_POLICIES_HPP_INCLUDED
#define BOOST_LOCKFREE_POLICIES_HPP_INCLUDED

#include <boost/config.hpp>
#ifdef BOOST_HAS_PRAGMA_ONCE
#    pragma once
#endif

#include <boost/parameter/template_keyword.hpp>

#include <type_traits>

namespace boost { namespace lockfree {

#ifndef BOOST_DOXYGEN_INVOKED
namespace tag {
struct allocator;
struct fixed_sized;
struct capacity;
struct allow_multiple_reads;
struct freelist;
struct single_producer;
struct single_consumer;
} // namespace tag

template < bool IsFixedSized >
struct fixed_sized : boost::parameter::template_keyword< tag::fixed_sized, std::integral_constant< bool, IsFixedSized > >
{};

template < size_t Size >
struct capacity : boost::parameter::template_keyword< tag::capacity, std::integral_constant< size_t, Size > >
{};

template < class Alloc >
struct allocator : boost::parameter::template_keyword< tag::allocator, Alloc >
{};

template < bool AllowMultipleReads >
struct allow_multiple_reads :
    boost::parameter::template_keyword< tag::allow_multiple_reads, std::integral_constant< bool, AllowMultipleReads > >
{};

template < bool UseFreelist >
struct freelist : boost::parameter::template_keyword< tag::freelist, std::integral_constant< bool, UseFreelist > >
{};

template < bool IsSingleProducer >
struct single_producer :
    boost::parameter::template_keyword< tag::single_producer, std::integral_constant< bool, IsSingleProducer > >
{};

template < bool IsSingleConsumer >
struct single_consumer :
    boost::parameter::template_keyword< tag::single_consumer, std::integral_constant< bool, IsSingleConsumer > >
{};

#else

/** Configures a data structure as \b fixed-sized.
 *
 *  The internal nodes are stored inside an array and they are addressed by array indexing. This limits the possible
 * size of the queue to the number of elements that can be addressed by the index type (usually 2**16-2), but on
 * platforms that lack double-width compare-and-exchange instructions, this is the best way to achieve lock-freedom.
 * This implies that a data structure is bounded.
 * */
template < bool IsFixedSized >
struct fixed_sized;

/** Sets the \b capacity of a data structure at compile-time.
 *
 * This implies that a data structure is bounded and fixed-sized.
 * */
template < size_t Size >
struct capacity;

/** Defines the \b allocator type of a data structure.
 * */
template < class Alloc >
struct allocator;

/** Configures the spsc_value to consume the value multiple times
 *
 * Caveats:
 * * one cannot move the value out
 * */
template < bool AllowMultipleReads >
struct allow_multiple_reads;

/** Configures the mpmc_queue to use a freelist.
 *
 * The mpmc_queue can be configured to use a freelist for memory management.
 *
 * */
template < bool UseFreelist >
struct freelist;

/** Configures a data structure for \b single-producer usage.
 *
 *  When set to true, the push operation does not use a CAS and is
 *  wait-free. Only one thread may call push concurrently.
 * */
template < bool IsSingleProducer >
struct single_producer;

/** Configures a data structure for \b single-consumer usage.
 *
 *  When set to true, the pop operation does not use a CAS and is
 *  wait-free. Only one thread may call pop concurrently.
 * */
template < bool IsSingleConsumer >
struct single_consumer;

#endif

}} // namespace boost::lockfree

#endif /* BOOST_LOCKFREE_POLICIES_HPP_INCLUDED */
