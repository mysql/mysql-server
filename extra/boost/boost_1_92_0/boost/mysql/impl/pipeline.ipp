//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_PIPELINE_IPP
#define BOOST_MYSQL_IMPL_PIPELINE_IPP

#pragma once

#include <boost/mysql/error_code.hpp>
#include <boost/mysql/field_view.hpp>
#include <boost/mysql/pipeline.hpp>

#include <boost/mysql/detail/access.hpp>
#include <boost/mysql/detail/pipeline.hpp>
#include <boost/mysql/detail/resultset_encoding.hpp>

#include <boost/mysql/impl/internal/protocol/serialization.hpp>
#include <boost/mysql/impl/internal/sansio/set_character_set.hpp>

#include <boost/core/span.hpp>
#include <boost/throw_exception.hpp>

#include <stdexcept>

boost::mysql::pipeline_request& boost::mysql::pipeline_request::add_execute(string_view query)
{
    impl_.stages_.reserve(impl_.stages_.size() + 1);  // strong guarantee
    impl_.stages_.push_back({
        detail::pipeline_stage_kind::execute,
        detail::serialize_top_level_checked(detail::query_command{query}, impl_.buffer_),
        detail::resultset_encoding::text,
    });
    return *this;
}

boost::mysql::pipeline_request& boost::mysql::pipeline_request::add_execute_range(
    statement stmt,
    span<const field_view> params
)
{
    if (params.size() != stmt.num_params())
    {
        BOOST_THROW_EXCEPTION(
            std::invalid_argument("Wrong number of actual parameters supplied to a prepared statement")
        );
    }
    impl_.stages_.reserve(impl_.stages_.size() + 1);  // strong guarantee
    impl_.stages_.push_back({
        detail::pipeline_stage_kind::execute,
        detail::serialize_top_level_checked(detail::execute_stmt_command{stmt.id(), params}, impl_.buffer_),
        detail::resultset_encoding::binary,
    });
    return *this;
}

boost::mysql::pipeline_request& boost::mysql::pipeline_request::add_prepare_statement(string_view stmt_sql)
{
    impl_.stages_.reserve(impl_.stages_.size() + 1);  // strong guarantee
    impl_.stages_.push_back({
        detail::pipeline_stage_kind::prepare_statement,
        detail::serialize_top_level_checked(detail::prepare_stmt_command{stmt_sql}, impl_.buffer_),
        {},
    });
    return *this;
}

boost::mysql::pipeline_request& boost::mysql::pipeline_request::add_close_statement(statement stmt)
{
    impl_.stages_.reserve(impl_.stages_.size() + 1);  // strong guarantee
    impl_.stages_.push_back({
        detail::pipeline_stage_kind::close_statement,
        detail::serialize_top_level_checked(detail::close_stmt_command{stmt.id()}, impl_.buffer_),
        {},
    });
    return *this;
}

boost::mysql::pipeline_request& boost::mysql::pipeline_request::add_reset_connection()
{
    impl_.stages_.reserve(impl_.stages_.size() + 1);  // strong guarantee
    impl_.stages_.push_back({
        detail::pipeline_stage_kind::reset_connection,
        detail::serialize_top_level_checked(detail::reset_connection_command{}, impl_.buffer_),
        {},
    });
    return *this;
}

boost::mysql::pipeline_request& boost::mysql::pipeline_request::add_set_character_set(character_set charset)
{
    auto q = detail::compose_set_names(charset);
    if (q.has_error())
    {
        BOOST_THROW_EXCEPTION(std::invalid_argument("Invalid character set name"));
    }
    impl_.stages_.reserve(impl_.stages_.size() + 1);  // strong guarantee
    impl_.stages_.push_back({
        detail::pipeline_stage_kind::set_character_set,
        detail::serialize_top_level_checked(detail::query_command{*q}, impl_.buffer_),
        charset,
    });
    return *this;
}

void boost::mysql::stage_response::check_has_results() const
{
    if (!has_results())
    {
        BOOST_THROW_EXCEPTION(
            std::invalid_argument("stage_response::as_results: object doesn't contain results")
        );
    }
}

boost::mysql::statement boost::mysql::stage_response::as_statement() const
{
    if (!has_statement())
    {
        BOOST_THROW_EXCEPTION(
            std::invalid_argument("stage_response::as_statement: object doesn't contain a statement")
        );
    }
    return variant2::unsafe_get<1>(impl_.value);
}

#endif
