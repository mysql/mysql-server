//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_CONNECT_PARAMS_HELPERS_HPP
#define BOOST_MYSQL_DETAIL_CONNECT_PARAMS_HELPERS_HPP

#include <boost/mysql/any_address.hpp>
#include <boost/mysql/connect_params.hpp>
#include <boost/mysql/handshake_params.hpp>
#include <boost/mysql/ssl_mode.hpp>

namespace boost {
namespace mysql {
namespace detail {

inline ssl_mode adjust_ssl_mode(ssl_mode input, address_type addr_type)
{
    return addr_type == address_type::host_and_port ? input : ssl_mode::disable;
}

inline handshake_params make_hparams(const connect_params& input)
{
    return handshake_params(
        input.username,
        input.password,
        input.database,
        input.connection_collation,
        adjust_ssl_mode(input.ssl, input.server_address.type()),
        input.multi_queries
    );
}

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
