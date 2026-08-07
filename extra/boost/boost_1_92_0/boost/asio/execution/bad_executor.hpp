//
// execution/bad_executor.hpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2026 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_ASIO_EXECUTION_BAD_EXECUTOR_HPP
#define BOOST_ASIO_EXECUTION_BAD_EXECUTOR_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include <boost/asio/detail/config.hpp>
#include <exception>
#include <boost/asio/detail/push_options.hpp>

namespace boost {
namespace asio {
BOOST_ASIO_INLINE_NAMESPACE_BEGIN
namespace execution {

/// Exception thrown when trying to access an empty polymorphic executor.
class bad_executor
  : public std::exception
{
public:
  /// Constructor.
  bad_executor() noexcept
  {
  }

  /// Destructor.
  virtual ~bad_executor() noexcept
  {
  }

  /// Obtain message associated with exception.
  virtual const char* what() const noexcept
  {
    return "bad executor";
  }
};

} // namespace execution
BOOST_ASIO_INLINE_NAMESPACE_END
} // namespace asio
} // namespace boost

#include <boost/asio/detail/pop_options.hpp>

#endif // BOOST_ASIO_EXECUTION_BAD_EXECUTOR_HPP
