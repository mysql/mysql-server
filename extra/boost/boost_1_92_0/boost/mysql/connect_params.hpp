//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_CONNECT_PARAMS_HPP
#define BOOST_MYSQL_CONNECT_PARAMS_HPP

#include <boost/mysql/any_address.hpp>
#include <boost/mysql/character_set.hpp>
#include <boost/mysql/ssl_mode.hpp>
#include <boost/mysql/string_view.hpp>

#include <cstdint>
#include <string>

namespace boost {
namespace mysql {

/**
 * \brief Parameters to be used with \ref any_connection connect functions.
 * \details
 * To be passed to \ref any_connection::connect and \ref any_connection::async_connect.
 * Includes the server address and MySQL handshake parameters. This is an owning type.
 */
struct connect_params
{
    /**
     * \brief Determines how to establish a physical connection to the MySQL server.
     * \details
     * This can be either a host and port or a UNIX socket path.
     * Defaults to (localhost, 3306).
     */
    any_address server_address;

    /// User name to authenticate as.
    std::string username;

    /// Password for that username, possibly empty.
    std::string password;

    /// Database name to use, or empty string for no database (this is the default).
    std::string database;

    /**
     * \brief The ID of the collation to use for the connection.
     * \details Impacts how text queries and prepared statements are interpreted. Defaults to
     * `utf8mb4_general_ci`, which is compatible with MySQL 5.x, 8.x and MariaDB.
     */
    std::uint16_t connection_collation{45};

    /**
     * \brief Controls whether to use TLS or not.
     * \details
     * See \ref ssl_mode for more information about the possible modes.
     * This option is only relevant when `server_address.type() == address_type::host_and_port`.
     * UNIX socket connections will never use TLS, regardless of this value.
     */
    ssl_mode ssl{ssl_mode::enable};

    /**
     * \brief Whether to enable support for executing semicolon-separated text queries.
     * \details Disabled by default.
     */
    bool multi_queries{false};

    /**
     * \brief The character set that the server uses as its default.
     * \details
     * Set this value if you know the character set that your server is configured to use
     * by default, as an optimization. It affects how the connection tracks its current
     * character set (see \ref any_connection::current_character_set):
     *
     * \li If \ref connection_collation is set to \ref invalid_collation_id,
     *     after connection establishment, the connection's character set is assumed to be
     *     this one.
     * \li After a successful \ref any_connection::async_reset_connection, the connection's
     *     character set is assumed to be this one (resetting restores the server's default).
     *
     * Leave it value-initialized (the default) if you don't know the server's default
     * character set, in which case the above operations leave the current character set unknown.
     *
     * When you set this value, you are promising the library that your server is configured
     * to use this character set as its default. This is not checked at runtime, and failing
     * to fulfill this promise can lead to vulnerabilities. Use it only as an optimization if
     * you know what you are doing.
     */
    character_set server_default_charset{};
};

}  // namespace mysql
}  // namespace boost

#endif
