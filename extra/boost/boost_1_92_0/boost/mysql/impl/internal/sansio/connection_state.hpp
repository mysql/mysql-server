//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_INTERNAL_SANSIO_CONNECTION_STATE_HPP
#define BOOST_MYSQL_IMPL_INTERNAL_SANSIO_CONNECTION_STATE_HPP

#include <boost/mysql/client_errc.hpp>
#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/handshake_params.hpp>
#include <boost/mysql/statement.hpp>

#include <boost/mysql/detail/algo_params.hpp>
#include <boost/mysql/detail/any_resumable_ref.hpp>
#include <boost/mysql/detail/next_action.hpp>

#include <boost/mysql/impl/internal/sansio/close_connection.hpp>
#include <boost/mysql/impl/internal/sansio/close_statement.hpp>
#include <boost/mysql/impl/internal/sansio/connect.hpp>
#include <boost/mysql/impl/internal/sansio/connection_state_data.hpp>
#include <boost/mysql/impl/internal/sansio/execute.hpp>
#include <boost/mysql/impl/internal/sansio/handshake.hpp>
#include <boost/mysql/impl/internal/sansio/ping.hpp>
#include <boost/mysql/impl/internal/sansio/prepare_statement.hpp>
#include <boost/mysql/impl/internal/sansio/quit_connection.hpp>
#include <boost/mysql/impl/internal/sansio/read_resultset_head.hpp>
#include <boost/mysql/impl/internal/sansio/read_some_rows.hpp>
#include <boost/mysql/impl/internal/sansio/read_some_rows_dynamic.hpp>
#include <boost/mysql/impl/internal/sansio/reset_connection.hpp>
#include <boost/mysql/impl/internal/sansio/run_pipeline.hpp>
#include <boost/mysql/impl/internal/sansio/set_character_set.hpp>
#include <boost/mysql/impl/internal/sansio/start_execution.hpp>
#include <boost/mysql/impl/internal/sansio/top_level_algo.hpp>

#include <boost/variant2/variant.hpp>

#include <cstddef>

namespace boost {
namespace mysql {
namespace detail {

// clang-format off
template <class AlgoParams> struct get_algo;
template <> struct get_algo<connect_algo_params> { using type = connect_algo; };
template <> struct get_algo<handshake_algo_params> { using type = handshake_algo; };
template <> struct get_algo<execute_algo_params> { using type = execute_algo; };
template <> struct get_algo<start_execution_algo_params> { using type = start_execution_algo; };
template <> struct get_algo<read_resultset_head_algo_params> { using type = read_resultset_head_algo; };
template <> struct get_algo<read_some_rows_algo_params> { using type = read_some_rows_algo; };
template <> struct get_algo<read_some_rows_dynamic_algo_params> { using type = read_some_rows_dynamic_algo; };
template <> struct get_algo<prepare_statement_algo_params> { using type = prepare_statement_algo; };
template <> struct get_algo<set_character_set_algo_params> { using type = set_character_set_algo; };
template <> struct get_algo<quit_connection_algo_params> { using type = quit_connection_algo; };
template <> struct get_algo<close_connection_algo_params> { using type = close_connection_algo; };
template <> struct get_algo<run_pipeline_algo_params> { using type = run_pipeline_algo; };
template <class AlgoParams> using get_algo_t = typename get_algo<AlgoParams>::type;
// clang-format on

class connection_state
{
    // Helper
    template <class... Algos>
    using make_any_algo_type = variant2::variant<top_level_algo<Algos>...>;

    using any_algo = make_any_algo_type<
        connect_algo,
        handshake_algo,
        execute_algo,
        start_execution_algo,
        read_resultset_head_algo,
        read_some_rows_algo,
        read_some_rows_dynamic_algo,
        prepare_statement_algo,
        set_character_set_algo,
        quit_connection_algo,
        close_connection_algo,
        run_pipeline_algo>;

    connection_state_data st_data_;
    any_algo algo_;

    // A function compatible with any_resumable_ref that always fails immediately
    // with operation_in_progress. We can't change algo_ while an algorithm is running,
    // so we need an any_resumable_ref that doesn't point into algo_
    static next_action fail_op_in_progress(void*, error_code, std::size_t)
    {
        return error_code(client_errc::operation_in_progress);
    }

public:
    // We initialize the algo state with a dummy value. This will be overwritten
    // by setup() before the first algorithm starts running. Doing this avoids
    // the need for a special null algo
    connection_state(std::size_t read_buffer_size, std::size_t max_buffer_size, bool transport_supports_ssl)
        : st_data_(read_buffer_size, max_buffer_size, transport_supports_ssl),
          algo_(top_level_algo<quit_connection_algo>(
              st_data_,
              st_data_.shared_diag,
              quit_connection_algo_params{}
          ))
    {
    }

    const connection_state_data& data() const { return st_data_; }
    connection_state_data& data() { return st_data_; }

    template <class AlgoParams>
    any_resumable_ref setup(diagnostics& diag, AlgoParams params)
    {
        // Clear diagnostics
        diag.clear();

        // If there is an operation in progress, don't change anything, just fail
        if (st_data_.op_in_progress)
            return any_resumable_ref(nullptr, &fail_op_in_progress);

        // Emplace the algorithm
        using algo_type = top_level_algo<get_algo_t<AlgoParams>>;
        return any_resumable_ref(algo_.emplace<algo_type>(st_data_, diag, params));
    }

    any_resumable_ref setup(diagnostics& diag, close_statement_algo_params params)
    {
        return setup(diag, setup_close_statement_pipeline(st_data_, params));
    }

    any_resumable_ref setup(diagnostics& diag, reset_connection_algo_params)
    {
        return setup(diag, setup_reset_connection_pipeline(st_data_));
    }

    any_resumable_ref setup(diagnostics& diag, ping_algo_params)
    {
        return setup(diag, setup_ping_pipeline(st_data_));
    }

    template <typename AlgoParams>
    typename AlgoParams::result_type result() const
    {
        return variant2::get<top_level_algo<get_algo_t<AlgoParams>>>(algo_).inner_algo().result(st_data_);
    }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
