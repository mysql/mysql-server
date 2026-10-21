//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DEFAULTS_HPP
#define BOOST_MYSQL_DEFAULTS_HPP

#include <boost/config.hpp>

#include <cstddef>
#include <cstdint>

namespace boost {
namespace mysql {

/// The default TCP port for the MySQL protocol.
BOOST_INLINE_CONSTEXPR unsigned short default_port = 3306;

/// The default TCP port for the MySQL protocol, as a string. Useful for hostname resolution.
BOOST_INLINE_CONSTEXPR const char* default_port_string = "3306";

/// The default initial size of the connection's internal buffer, in bytes.
BOOST_INLINE_CONSTEXPR std::size_t default_initial_read_buffer_size = 1024;

/**
 * \brief A collation ID that is never valid.
 * \details
 * Pass this value as \ref connect_params::connection_collation to set the
 * connection's character set and collation to the server's defaults.
 * Run `"SELECT @@GLOBAL.character_set_client;"` to find out the server's
 * default character set.
 *
 * This is intended to be used together with \ref connect_params::server_default_charset.
 */
BOOST_INLINE_CONSTEXPR std::uint16_t invalid_collation_id = 0u;

}  // namespace mysql
}  // namespace boost

#endif
