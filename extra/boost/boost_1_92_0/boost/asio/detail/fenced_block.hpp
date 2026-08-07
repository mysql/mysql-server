//
// detail/fenced_block.hpp
// ~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2026 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_ASIO_DETAIL_FENCED_BLOCK_HPP
#define BOOST_ASIO_DETAIL_FENCED_BLOCK_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include <boost/asio/detail/config.hpp>

#if !defined(BOOST_ASIO_HAS_THREADS) \
  || defined(BOOST_ASIO_DISABLE_FENCED_BLOCK) \
  || defined(BOOST_ASIO_HAS_THREAD_SANITIZER)
# include <boost/asio/detail/null_fenced_block.hpp>
#else
# include <boost/asio/detail/std_fenced_block.hpp>
#endif

namespace boost {
namespace asio {
BOOST_ASIO_INLINE_NAMESPACE_BEGIN
namespace detail {

#if !defined(BOOST_ASIO_HAS_THREADS) \
  || defined(BOOST_ASIO_DISABLE_FENCED_BLOCK) \
  || defined(BOOST_ASIO_HAS_THREAD_SANITIZER)
typedef null_fenced_block fenced_block;
#else
typedef std_fenced_block fenced_block;
#endif

} // namespace detail
BOOST_ASIO_INLINE_NAMESPACE_END
} // namespace asio
} // namespace boost

#endif // BOOST_ASIO_DETAIL_FENCED_BLOCK_HPP
