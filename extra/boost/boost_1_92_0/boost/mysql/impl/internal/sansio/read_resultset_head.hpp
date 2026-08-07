//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_INTERNAL_SANSIO_READ_RESULTSET_HEAD_HPP
#define BOOST_MYSQL_IMPL_INTERNAL_SANSIO_READ_RESULTSET_HEAD_HPP

#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>

#include <boost/mysql/detail/algo_params.hpp>
#include <boost/mysql/detail/execution_processor/execution_processor.hpp>
#include <boost/mysql/detail/next_action.hpp>

#include <boost/mysql/impl/internal/coroutine.hpp>
#include <boost/mysql/impl/internal/sansio/connection_state_data.hpp>

namespace boost {
namespace mysql {
namespace detail {

inline error_code process_execution_response(
    connection_state_data& st,
    execution_processor& proc,
    span<const std::uint8_t> msg,
    diagnostics& diag
)
{
    auto response = deserialize_execute_response(msg, st.flavor, diag);
    error_code err;
    switch (response.type)
    {
    case execute_response::type_t::error: err = response.data.err; break;
    case execute_response::type_t::ok_packet:
        st.backslash_escapes = response.data.ok_pack.backslash_escapes();
        err = proc.on_head_ok_packet(response.data.ok_pack, diag);
        break;
    case execute_response::type_t::num_fields: proc.on_num_meta(response.data.num_fields); break;
    }
    return err;
}

inline error_code process_field_definition(
    execution_processor& proc,
    span<const std::uint8_t> msg,
    diagnostics& diag
)
{
    // Deserialize the message
    coldef_view coldef{};
    auto err = deserialize_column_definition(msg, coldef);
    if (err)
        return err;

    // Notify the processor
    return proc.on_meta(coldef, diag);
}

class read_resultset_head_algo
{
    execution_processor* proc_;
    bool is_top_level_;

    struct state_t
    {
        int resume_point{0};
    } state_;

    // Status changes are only performed if we're the top-level algorithm.
    // After an error, multi-function operations are considered finished
    void maybe_set_status_ready(connection_state_data& st) const
    {
        if (is_top_level_)
            st.status = connection_status::ready;
    }

public:
    read_resultset_head_algo(read_resultset_head_algo_params params, bool is_top_level = true) noexcept
        : proc_(params.proc), is_top_level_(is_top_level)
    {
    }

    void reset() { state_ = state_t{}; }

    execution_processor& processor() { return *proc_; }

    next_action resume(connection_state_data& st, diagnostics& diag, error_code ec)
    {
        switch (state_.resume_point)
        {
        case 0:

            // If we're not reading head, return (for compatibility, we don't error here).
            if (!proc_->is_reading_head())
                return next_action();

            // Check connection status. The check is only correct if we're the top-level algorithm
            if (is_top_level_)
            {
                ec = st.check_status_multi_function();
                if (ec)
                    return ec;
            }

            // Read the response
            BOOST_MYSQL_YIELD(state_.resume_point, 1, st.read(proc_->sequence_number()))
            if (ec)
            {
                maybe_set_status_ready(st);
                return ec;
            }

            // Response may be: ok_packet, err_packet, local infile request
            // (not implemented), or response with fields
            ec = process_execution_response(st, *proc_, st.reader.message(), diag);
            if (ec)
            {
                maybe_set_status_ready(st);
                return ec;
            }

            // Read all of the field definitions
            while (proc_->is_reading_meta())
            {
                // Read a message
                BOOST_MYSQL_YIELD(state_.resume_point, 2, st.read(proc_->sequence_number()))
                if (ec)
                {
                    maybe_set_status_ready(st);
                    return ec;
                }

                // Process the metadata packet
                ec = process_field_definition(*proc_, st.reader.message(), diag);
                if (ec)
                {
                    maybe_set_status_ready(st);
                    return ec;
                }
            }

            // No EOF packet is expected here, as we require deprecate EOF capabilities

            // If the received the final OK packet, we're no longer running a multi-function op
            if (proc_->is_complete() && is_top_level_)
                st.status = connection_status::ready;
        }

        return next_action();
    }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
