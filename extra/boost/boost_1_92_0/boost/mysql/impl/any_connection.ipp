//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_ANY_CONNECTION_IPP
#define BOOST_MYSQL_IMPL_ANY_CONNECTION_IPP

#pragma once

#include <boost/mysql/any_connection.hpp>

#include <boost/mysql/detail/engine.hpp>
#include <boost/mysql/detail/engine_impl.hpp>

#include <boost/mysql/impl/internal/variant_stream.hpp>

std::unique_ptr<boost::mysql::detail::engine> boost::mysql::any_connection::create_engine(
    asio::any_io_executor ex,
    asio::ssl::context* ctx
)
{
    return std::unique_ptr<detail::engine>(new detail::engine_impl<detail::variant_stream>(std::move(ex), ctx)
    );
}

#endif
