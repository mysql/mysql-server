//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_BUFFER_PARAMS_HPP
#define BOOST_MYSQL_BUFFER_PARAMS_HPP

#include <boost/mysql/defaults.hpp>

#include <boost/config.hpp>

#include <cstddef>

namespace boost {
namespace mysql {

/**
 * \brief (Legacy) Buffer configuration parameters for a connection.
 *
 * \par Legacy
 * This class is used with the legacy \ref connection class.
 * New code should use \ref any_connection, instead.
 * The equivalent to `buffer_params` is \ref any_connection_params::initial_buffer_size.
 */
class buffer_params
{
    std::size_t initial_read_size_;

public:
    /// The default value of \ref initial_read_size.
    static BOOST_INLINE_CONSTEXPR std::size_t default_initial_read_size = default_initial_read_buffer_size;

    /**
     * \brief Initializing constructor.
     * \param initial_read_size Initial size of the read buffer. A bigger read buffer
     * can increase the number of rows returned by \ref connection::read_some_rows.
     */
    constexpr explicit buffer_params(std::size_t initial_read_size = default_initial_read_size) noexcept
        : initial_read_size_(initial_read_size)
    {
    }

    /// Gets the initial size of the read buffer.
    constexpr std::size_t initial_read_size() const noexcept { return initial_read_size_; }

    /// Sets the initial size of the read buffer.
    void set_initial_read_size(std::size_t v) noexcept { initial_read_size_ = v; }
};

}  // namespace mysql
}  // namespace boost

#endif
