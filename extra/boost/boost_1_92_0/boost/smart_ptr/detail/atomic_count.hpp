#ifndef BOOST_SMART_PTR_DETAIL_ATOMIC_COUNT_HPP_INCLUDED
#define BOOST_SMART_PTR_DETAIL_ATOMIC_COUNT_HPP_INCLUDED

// MS compatible compilers support #pragma once

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
#endif

//
//  boost/detail/atomic_count.hpp - thread/SMP safe reference counter
//
//  Copyright (c) 2001, 2002 Peter Dimov and Multi Media Ltd.
//  Copyright (c) 2013 Peter Dimov
//
//  Distributed under the Boost Software License, Version 1.0.
//  See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt
//
//  typedef <implementation-defined> boost::detail::atomic_count;
//
//  atomic_count a(n);
//
//    (n is convertible to long)
//
//    Effects: Constructs an atomic_count with an initial value of n
//
//  a;
//
//    Returns: (long) the current value of a
//    Memory Ordering: acquire
//
//  ++a;
//
//    Effects: Atomically increments the value of a
//    Returns: (long) the new value of a
//    Memory Ordering: acquire/release
//
//  --a;
//
//    Effects: Atomically decrements the value of a
//    Returns: (long) the new value of a
//    Memory Ordering: acquire/release
//

#include <boost/smart_ptr/detail/deprecated_macros.hpp>

#if defined( BOOST_AC_DISABLE_THREADS )
# include <boost/smart_ptr/detail/atomic_count_nt.hpp>

#elif defined( BOOST_AC_USE_STD_ATOMIC )
# include <boost/smart_ptr/detail/atomic_count_std_atomic.hpp>

#elif defined( BOOST_SP_DISABLE_THREADS )
# include <boost/smart_ptr/detail/atomic_count_nt.hpp>

#else
# include <boost/smart_ptr/detail/atomic_count_std_atomic.hpp>

#endif

#endif // #ifndef BOOST_SMART_PTR_DETAIL_ATOMIC_COUNT_HPP_INCLUDED
