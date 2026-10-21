//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_CONNECTION_POOL_IPP
#define BOOST_MYSQL_IMPL_CONNECTION_POOL_IPP

#pragma once

#include <boost/mysql/connection_pool.hpp>

#include <boost/mysql/detail/connection_pool_fwd.hpp>

#include <boost/mysql/impl/internal/connection_pool/connection_pool_impl.hpp>

#include <boost/asio/any_io_executor.hpp>

#include <memory>

void boost::mysql::detail::return_connection(
    pool_impl& pool,
    connection_node& node,
    bool should_reset
) noexcept
{
    pool.return_connection(node, should_reset);
}

boost::mysql::any_connection& boost::mysql::detail::get_connection(boost::mysql::detail::connection_node& node
) noexcept
{
    return node.connection();
}

boost::mysql::connection_pool::connection_pool(asio::any_io_executor ex, pool_params&& params, int)
    : impl_(std::make_shared<detail::pool_impl>(std::move(ex), std::move(params)))
{
}

boost::mysql::connection_pool::executor_type boost::mysql::connection_pool::get_executor() noexcept
{
    return impl_->get_executor();
}

void boost::mysql::connection_pool::async_run_erased(
    std::shared_ptr<detail::pool_impl> pool,
    asio::any_completion_handler<void(error_code)> handler
)
{
    pool->async_run(std::move(handler));
}

void boost::mysql::connection_pool::async_get_connection_erased(
    std::shared_ptr<detail::pool_impl> pool,
    diagnostics* diag,
    asio::any_completion_handler<void(error_code, pooled_connection)> handler
)
{
    pool->async_get_connection(diag, std::move(handler));
}

void boost::mysql::connection_pool::cancel()
{
    BOOST_ASSERT(valid());
    impl_->cancel();
}

#endif
