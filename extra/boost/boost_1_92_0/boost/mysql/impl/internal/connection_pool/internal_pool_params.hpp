//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_INTERNAL_CONNECTION_POOL_INTERNAL_POOL_PARAMS_HPP
#define BOOST_MYSQL_IMPL_INTERNAL_CONNECTION_POOL_INTERNAL_POOL_PARAMS_HPP

#include <boost/mysql/any_connection.hpp>
#include <boost/mysql/character_set.hpp>
#include <boost/mysql/connect_params.hpp>
#include <boost/mysql/defaults.hpp>
#include <boost/mysql/handshake_params.hpp>
#include <boost/mysql/pipeline.hpp>
#include <boost/mysql/pool_params.hpp>
#include <boost/mysql/ssl_mode.hpp>

#include <boost/asio/ssl/context.hpp>
#include <boost/optional/optional.hpp>
#include <boost/throw_exception.hpp>

#include <chrono>
#include <cstddef>
#include <stdexcept>
#include <string>

namespace boost {
namespace mysql {
namespace detail {

// Same as pool_params, but structured in a way that is more helpful for the impl
struct internal_pool_params
{
    connect_params connect_config;
    pipeline_request reset_pipeline;
    optional<asio::ssl::context> ssl_ctx;
    std::size_t initial_buffer_size;
    std::size_t initial_size;
    std::size_t max_size;
    std::chrono::steady_clock::duration connect_timeout;
    std::chrono::steady_clock::duration ping_timeout;
    std::chrono::steady_clock::duration retry_interval;
    std::chrono::steady_clock::duration ping_interval;
    bool thread_safe;

    any_connection_params make_ctor_params() noexcept
    {
        any_connection_params res;
        res.ssl_context = ssl_ctx.get_ptr();
        res.initial_buffer_size = initial_buffer_size;
        return res;
    }
};

inline void check_validity(const pool_params& params)
{
    const char* msg = nullptr;
    if (params.max_size == 0)
        msg = "pool_params::max_size must be greater than zero";
    else if (params.max_size < params.initial_size)
        msg = "pool_params::max_size must be greater than pool_params::initial_size";
    else if (params.connect_timeout.count() < 0)
        msg = "pool_params::connect_timeout must not be negative";
    else if (params.retry_interval.count() <= 0)
        msg = "pool_params::retry_interval must be greater than zero";
    else if (params.ping_interval.count() < 0)
        msg = "pool_params::ping_interval must not be negative";
    else if (params.ping_timeout.count() < 0)
        msg = "pool_params::ping_timeout must not be negative";

    if (msg != nullptr)
    {
        BOOST_THROW_EXCEPTION(std::invalid_argument(msg));
    }
}

inline internal_pool_params make_internal_pool_params(pool_params&& params)
{
    check_validity(params);

    connect_params connect_prms;
    connect_prms.server_address = std::move(params.server_address);
    connect_prms.username = std::move(params.username);
    connect_prms.password = std::move(params.password);
    connect_prms.database = std::move(params.database);
    connect_prms.ssl = params.ssl;
    connect_prms.multi_queries = params.multi_queries;
    if (params.charset_strategy.type() == pool_charset_strategy_type::use_server_default)
    {
        // Intentionally invalid, to force the server use its default.
        // We can't allow otherwise because resetting sets the collation back to the default.
        connect_prms.connection_collation = invalid_collation_id;
        connect_prms.server_default_charset = params.charset_strategy.server_default_charset();
    }

    pipeline_request reset_pipe;
    reset_pipe.add_reset_connection();
    if (params.charset_strategy.type() == pool_charset_strategy_type::set_to_utf8mb4)
        reset_pipe.add_set_character_set(utf8mb4_charset);

    return {
        std::move(connect_prms),
        std::move(reset_pipe),
        std::move(params.ssl_ctx),
        params.initial_buffer_size,
        params.initial_size,
        params.max_size,
        params.connect_timeout,
        params.ping_timeout,
        params.retry_interval,
        params.ping_interval,
        params.thread_safe,
    };
}

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
