//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_METADATA_MODE_HPP
#define BOOST_MYSQL_METADATA_MODE_HPP

namespace boost {
namespace mysql {

/// Describes how to handle metadata when running a query or statement.
enum class metadata_mode
{
    /**
     * \brief Retain the minimum metadata possible to be able to execute the operation.
     * This is the most efficient mode, but will leave some fields in the \ref metadata
     * class empty.
     */
    minimal,

    /**
     * \brief Retain as much metadata as possible. All the fields in \ref metadata are usable,
     * but causes more allocations.
     */
    full
};

}  // namespace mysql
}  // namespace boost

#endif
