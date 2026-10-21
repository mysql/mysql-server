//
// detail/slim_mutex.hpp
// ~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2026 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_ASIO_DETAIL_SLIM_MUTEX_HPP
#define BOOST_ASIO_DETAIL_SLIM_MUTEX_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include <boost/asio/detail/config.hpp>

#if !defined(BOOST_ASIO_HAS_THREADS)
# include <boost/asio/detail/null_mutex.hpp>
#elif defined(BOOST_ASIO_HAS_FUTEX)
# include <boost/asio/detail/futex_slim_mutex.hpp>
#elif defined(BOOST_ASIO_HAS_STD_ATOMIC_WAIT)
# include <boost/asio/detail/atomic_slim_mutex.hpp>
#else
# include <boost/asio/detail/mutex.hpp>
#endif

namespace boost {
namespace asio {
BOOST_ASIO_INLINE_NAMESPACE_BEGIN
namespace detail {

#if !defined(BOOST_ASIO_HAS_THREADS)
typedef null_mutex slim_mutex;
#elif defined(BOOST_ASIO_HAS_FUTEX)
typedef futex_slim_mutex slim_mutex;
#elif defined(BOOST_ASIO_HAS_STD_ATOMIC_WAIT)
typedef atomic_slim_mutex slim_mutex;
#else
typedef mutex slim_mutex;
#endif

} // namespace detail
BOOST_ASIO_INLINE_NAMESPACE_END
} // namespace asio
} // namespace boost

#endif // BOOST_ASIO_DETAIL_SLIM_MUTEX_HPP
