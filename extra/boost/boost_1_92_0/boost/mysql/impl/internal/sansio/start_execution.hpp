//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_INTERNAL_SANSIO_START_EXECUTION_HPP
#define BOOST_MYSQL_IMPL_INTERNAL_SANSIO_START_EXECUTION_HPP

#include <boost/mysql/character_set.hpp>
#include <boost/mysql/client_errc.hpp>
#include <boost/mysql/constant_string_view.hpp>
#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/format_sql.hpp>

#include <boost/mysql/detail/algo_params.hpp>
#include <boost/mysql/detail/any_execution_request.hpp>
#include <boost/mysql/detail/execution_processor/execution_processor.hpp>
#include <boost/mysql/detail/next_action.hpp>
#include <boost/mysql/detail/output_string.hpp>
#include <boost/mysql/detail/resultset_encoding.hpp>

#include <boost/mysql/impl/internal/coroutine.hpp>
#include <boost/mysql/impl/internal/protocol/impl/serialization_context.hpp>
#include <boost/mysql/impl/internal/protocol/serialization.hpp>
#include <boost/mysql/impl/internal/sansio/connection_state_data.hpp>
#include <boost/mysql/impl/internal/sansio/read_resultset_head.hpp>

#include <boost/core/span.hpp>

namespace boost {
namespace mysql {
namespace detail {

// A serializable to generate the query in the write buffer without copies
struct query_with_params
{
    constant_string_view query;
    span<const format_arg> args;
    format_options opts;

    void serialize(serialization_context& ctx) const
    {
        // Create a format context
        auto fmt_ctx = access::construct<format_context_base>(output_string_ref::create(ctx), opts);

        // Serialize the query header
        ctx.add(0x03);

        // Serialize the actual query
        vformat_sql_to(fmt_ctx, query, args);

        // Check for errors
        ctx.add_error(fmt_ctx.error_state());
    }
};

class start_execution_algo
{
    int resume_point_{0};
    read_resultset_head_algo read_head_st_;
    any_execution_request req_;
    bool is_top_level_;

    std::uint8_t& seqnum() { return processor().sequence_number(); }
    execution_processor& processor() { return read_head_st_.processor(); }

    static resultset_encoding get_encoding(any_execution_request::type_t type)
    {
        switch (type)
        {
        case any_execution_request::type_t::query:
        case any_execution_request::type_t::query_with_params: return resultset_encoding::text;
        case any_execution_request::type_t::stmt: return resultset_encoding::binary;
        default: BOOST_ASSERT(false); return resultset_encoding::text;  // LCOV_EXCL_LINE
        }
    }

    next_action write_query_with_params(
        connection_state_data& st,
        any_execution_request::data_t::query_with_params_t data
    )
    {
        // Determine format options
        if (st.current_charset.name == nullptr)
        {
            return error_code(client_errc::unknown_character_set);
        }
        format_options opts{st.current_charset, st.backslash_escapes};

        // Write the request
        return st.write(query_with_params{data.query, data.args, opts}, seqnum());
    }

    next_action write_stmt(connection_state_data& st, any_execution_request::data_t::stmt_t data)
    {
        if (data.num_params != data.params.size())
            return error_code(client_errc::wrong_num_params);
        return st.write(execute_stmt_command{data.stmt_id, data.params}, seqnum());
    }

    next_action compose_request(connection_state_data& st)
    {
        switch (req_.type)
        {
        case any_execution_request::type_t::query: return st.write(query_command{req_.data.query}, seqnum());
        case any_execution_request::type_t::query_with_params:
            return write_query_with_params(st, req_.data.query_with_params);
        case any_execution_request::type_t::stmt: return write_stmt(st, req_.data.stmt);
        default: BOOST_ASSERT(false); return next_action();  // LCOV_EXCL_LINE
        }
    }

public:
    // We pass false to the read head algo's constructor because it's a subordinate algos.
    // This suppresses state checks.
    start_execution_algo(start_execution_algo_params params, bool is_top_level = true) noexcept
        : read_head_st_({params.proc}, false), req_(params.req), is_top_level_(is_top_level)
    {
    }

    next_action resume(connection_state_data& st, diagnostics& diag, error_code ec)
    {
        next_action act;

        switch (resume_point_)
        {
        case 0:
            // Check connection status. The check is only correct if we're the top-level algorithm
            if (is_top_level_)
            {
                ec = st.check_status_ready();
                if (ec)
                    return ec;
            }

            // Reset the processor
            processor().reset(get_encoding(req_.type), st.meta_mode);

            // Send the execution request
            BOOST_MYSQL_YIELD(resume_point_, 1, compose_request(st))
            if (ec)
                return ec;

            // If the request was sent successfully, and we're the top-level algorithm,
            // we're now running a multi-function operation. The status flag is cleared
            // by the other algorithms on error or when an OK packet is received
            if (is_top_level_)
                st.status = connection_status::engaged_in_multi_function;

            // Read the first resultset's head and return its result
            while (!(act = read_head_st_.resume(st, diag, ec)).is_done())
                BOOST_MYSQL_YIELD(resume_point_, 2, act)

            // If there was an error, we're no longer running a multi-function operation
            if (act.error())
            {
                if (is_top_level_)
                    st.status = connection_status::ready;
                return act;
            }

            // If we received the final OK packet, we're no longer running a multi-function operation
            if (processor().is_complete() && is_top_level_)
                st.status = connection_status::ready;
        }

        return next_action();
    }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
