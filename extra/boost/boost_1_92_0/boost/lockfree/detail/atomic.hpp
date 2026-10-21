//  Copyright (C) 2011-2013, 2016 Tim Blechmann
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_LOCKFREE_DETAIL_ATOMIC_HPP
#define BOOST_LOCKFREE_DETAIL_ATOMIC_HPP

#if defined( BOOST_LOCKFREE_FORCE_BOOST_ATOMIC )
#    include <boost/atomic.hpp>
#else
#    include <atomic>
#endif

namespace boost { namespace lockfree {
namespace detail {

#if defined( BOOST_LOCKFREE_FORCE_BOOST_ATOMIC )
using boost::atomic;
using boost::memory_order_acq_rel;
using boost::memory_order_acquire;
using boost::memory_order_relaxed;
using boost::memory_order_release;
#else
using std::atomic;
using std::memory_order_acq_rel;
using std::memory_order_acquire;
using std::memory_order_relaxed;
using std::memory_order_release;
#endif

} // namespace detail
using detail::atomic;
using detail::memory_order_acq_rel;
using detail::memory_order_acquire;
using detail::memory_order_relaxed;
using detail::memory_order_release;

}} // namespace boost::lockfree

#endif /* BOOST_LOCKFREE_DETAIL_ATOMIC_HPP */
