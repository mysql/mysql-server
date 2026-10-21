//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IS_FATAL_ERROR_HPP
#define BOOST_MYSQL_IS_FATAL_ERROR_HPP

#include <boost/mysql/error_code.hpp>

#include <boost/mysql/detail/config.hpp>

namespace boost {
namespace mysql {

/**
 * \brief Checks whether an error requires re-connection.
 * \details
 * After an operation on an established connection (like executing a query) fails,
 * the connection may be usable for further operations (if the error was non-fatal)
 * or not (if the error was fatal). This function determines whether an error
 * code returned by a connection operation is fatal or not.
 * \n
 * To recover from a fatal error code, close and re-establish the connection.
 *
 * \par Exception safety
 * No-throw guarantee.
 */
BOOST_MYSQL_DECL
bool is_fatal_error(error_code ec) noexcept;

}  // namespace mysql
}  // namespace boost

#ifdef BOOST_MYSQL_HEADER_ONLY
#include <boost/mysql/impl/is_fatal_error.ipp>
#endif

#endif
