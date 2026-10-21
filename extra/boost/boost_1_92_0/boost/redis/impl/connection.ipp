/* Copyright (c) 2018-2025 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <boost/redis/connection.hpp>
#include <boost/redis/impl/log_to_file.hpp>

#include <cstddef>
#include <cstdio>
#include <string_view>
#include <utility>

namespace boost::redis {

logger detail::make_stderr_logger(logger::level lvl, std::string prefix)
{
   return logger(lvl, [prefix = std::move(prefix)](logger::level, std::string_view msg) {
      log_to_file(stderr, msg, prefix.c_str());
   });
}

connection::connection(executor_type ex, asio::ssl::context ctx, logger lgr)
: impl_{std::move(ex), std::move(ctx), std::move(lgr)}
{ }

void connection::async_run_impl(
   config const& cfg,
   logger&& l,
   asio::any_completion_handler<void(boost::system::error_code)> token)
{
   // Avoid calling the basic_connection::async_run overload taking a logger
   // because it generates deprecated messages when building this file
   impl_.set_stderr_logger(l.lvl, cfg);
   impl_.async_run(cfg, std::move(token));
}

void connection::async_run_impl(
   config const& cfg,
   asio::any_completion_handler<void(boost::system::error_code)> token)
{
   impl_.async_run(cfg, std::move(token));
}

void connection::async_exec_impl(
   request const& req,
   any_adapter&& adapter,
   asio::any_completion_handler<void(boost::system::error_code, std::size_t)> token)
{
   impl_.async_exec(req, std::move(adapter), std::move(token));
}

void connection::async_receive2_impl(
   asio::any_completion_handler<void(boost::system::error_code)> token)
{
   impl_.async_receive2(std::move(token));
}

void connection::cancel(operation op) { impl_.cancel(op); }

}  // namespace boost::redis
