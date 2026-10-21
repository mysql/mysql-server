//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_INTERNAL_SANSIO_CLOSE_STATEMENT_HPP
#define BOOST_MYSQL_IMPL_INTERNAL_SANSIO_CLOSE_STATEMENT_HPP

#include <boost/mysql/detail/algo_params.hpp>
#include <boost/mysql/detail/pipeline.hpp>

#include <boost/mysql/impl/internal/protocol/serialization.hpp>
#include <boost/mysql/impl/internal/sansio/connection_state_data.hpp>

namespace boost {
namespace mysql {
namespace detail {

inline run_pipeline_algo_params setup_close_statement_pipeline(
    connection_state_data& st,
    close_statement_algo_params params
)
{
    // Pipeline a ping with the close statement, to avoid delays on old connections
    // that don't set tcp_nodelay. Both requests are small and fixed size, so
    // we don't enforce any buffer limits here.
    st.write_buffer.clear();
    auto seqnum1 = serialize_top_level_checked(close_stmt_command{params.stmt_id}, st.write_buffer);
    auto seqnum2 = serialize_top_level_checked(ping_command{}, st.write_buffer);
    st.shared_pipeline_stages = {
        {
         {pipeline_stage_kind::close_statement, seqnum1, {}},
         {pipeline_stage_kind::ping, seqnum2, {}},
         }
    };
    return {st.write_buffer, st.shared_pipeline_stages, nullptr};
}

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
