//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_ALGO_PARAMS_HPP
#define BOOST_MYSQL_DETAIL_ALGO_PARAMS_HPP

#include <boost/mysql/character_set.hpp>
#include <boost/mysql/handshake_params.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/mysql/detail/any_execution_request.hpp>
#include <boost/mysql/detail/execution_processor/execution_processor.hpp>

#include <boost/core/span.hpp>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace boost {
namespace mysql {

class rows_view;
class statement;
class stage_response;

namespace detail {

class execution_processor;
class execution_state_impl;
struct pipeline_request_stage;

struct connect_algo_params
{
    const void* server_address;  // Points to an any_address or an endpoint for the corresponding stream. For
                                 // the templated connection, only valid until the first yield!
    handshake_params hparams;
    bool secure_channel;  // Are we using UNIX sockets or any other secure channel?
    character_set server_default_charset;

    using result_type = void;
};

struct handshake_algo_params
{
    handshake_params hparams;
    bool secure_channel;  // Are we using UNIX sockets or any other secure channel?
    character_set server_default_charset;

    using result_type = void;
};

struct execute_algo_params
{
    any_execution_request req;
    execution_processor* proc;

    using result_type = void;
};

struct start_execution_algo_params
{
    any_execution_request req;
    execution_processor* proc;

    using result_type = void;
};

struct read_resultset_head_algo_params
{
    execution_processor* proc;

    using result_type = void;
};

struct read_some_rows_algo_params
{
    execution_processor* proc;
    output_ref output;

    using result_type = std::size_t;
};

struct read_some_rows_dynamic_algo_params
{
    execution_state_impl* exec_st;

    using result_type = rows_view;
};

struct prepare_statement_algo_params
{
    string_view stmt_sql;

    using result_type = statement;
};

struct close_statement_algo_params
{
    std::uint32_t stmt_id;

    using result_type = void;
};

struct ping_algo_params
{
    using result_type = void;
};

struct reset_connection_algo_params
{
    using result_type = void;
};

struct set_character_set_algo_params
{
    character_set charset;

    using result_type = void;
};

struct quit_connection_algo_params
{
    using result_type = void;
};

struct close_connection_algo_params
{
    using result_type = void;
};

struct run_pipeline_algo_params
{
    span<const std::uint8_t> request_buffer;
    span<const pipeline_request_stage> request_stages;
    std::vector<stage_response>* response;

    using result_type = void;
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
