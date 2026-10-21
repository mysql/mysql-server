//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_INTERNAL_SANSIO_READ_SOME_ROWS_HPP
#define BOOST_MYSQL_IMPL_INTERNAL_SANSIO_READ_SOME_ROWS_HPP

#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/field_view.hpp>

#include <boost/mysql/detail/algo_params.hpp>
#include <boost/mysql/detail/execution_processor/execution_processor.hpp>
#include <boost/mysql/detail/next_action.hpp>

#include <boost/mysql/impl/internal/coroutine.hpp>
#include <boost/mysql/impl/internal/protocol/deserialization.hpp>
#include <boost/mysql/impl/internal/sansio/connection_state_data.hpp>

#include <cstddef>

namespace boost {
namespace mysql {
namespace detail {

class read_some_rows_algo
{
    execution_processor* proc_;
    output_ref output_;
    bool is_top_level_;

    struct state_t
    {
        int resume_point{0};
        std::size_t rows_read{0};
    } state_;

    BOOST_ATTRIBUTE_NODISCARD static std::pair<error_code, std::size_t> process_some_rows(
        connection_state_data& st,
        execution_processor& proc,
        output_ref output,
        diagnostics& diag
    )
    {
        // Process all read messages until they run out, an error happens
        // or an EOF is received
        std::size_t read_rows = 0;
        error_code err;
        proc.on_row_batch_start();
        while (true)
        {
            // Check for errors (like seqnum mismatches)
            if (st.reader.error())
                return {st.reader.error(), read_rows};

            // Get the row message
            auto buff = st.reader.message();

            // Deserialize it
            auto res = deserialize_row_message(buff, st.flavor, diag);
            if (res.type == row_message::type_t::error)
            {
                err = res.data.err;
            }
            else if (res.type == row_message::type_t::row)
            {
                output.set_offset(read_rows);
                err = proc.on_row(res.data.row, output, st.shared_fields);
                if (!err)
                    ++read_rows;
            }
            else
            {
                st.backslash_escapes = res.data.ok_pack.backslash_escapes();
                err = proc.on_row_ok_packet(res.data.ok_pack);
            }

            if (err)
                return {err, read_rows};

            // TODO: can we make this better?
            if (!proc.is_reading_rows() || read_rows >= output.max_size())
                break;

            // Attempt to parse the next message
            st.reader.prepare_read(proc.sequence_number());
            if (!st.reader.done())
                break;
        }
        proc.on_row_batch_finish();
        return {error_code(), read_rows};
    }

    // Status changes are only performed if we're the top-level algorithm.
    // After an error, multi-function operations are considered finished
    void maybe_set_status_ready(connection_state_data& st) const
    {
        if (is_top_level_)
            st.status = connection_status::ready;
    }

public:
    read_some_rows_algo(read_some_rows_algo_params params, bool is_top_level = true) noexcept
        : proc_(params.proc), output_(params.output), is_top_level_(is_top_level)
    {
    }

    void reset() { state_ = state_t{}; }

    const execution_processor& processor() const { return *proc_; }
    execution_processor& processor() { return *proc_; }

    next_action resume(connection_state_data& st, diagnostics& diag, error_code ec)
    {
        switch (state_.resume_point)
        {
        case 0:

            // Clear any previous use of shared fields.
            // Required for the dynamic version to work.
            st.shared_fields.clear();

            // If we are not reading rows, return (for compatibility, we don't error here)
            if (!processor().is_reading_rows())
                return next_action();

            // Check connection status. The check is only correct if we're the top-level algorithm
            if (is_top_level_)
            {
                ec = st.check_status_multi_function();
                if (ec)
                    return ec;
            }

            // Read at least one message. Keep parsing state, in case a previous message
            // was parsed partially
            BOOST_MYSQL_YIELD(state_.resume_point, 1, st.read(proc_->sequence_number(), true))
            if (ec)
            {
                // If there was an error reading the message, we're no longer in a multi-function operation
                maybe_set_status_ready(st);
                return ec;
            }

            // Process messages
            std::tie(ec, state_.rows_read) = process_some_rows(st, *proc_, output_, diag);
            if (ec)
            {
                // If there was an error parsing the message, we're no longer in a multi-function operation
                maybe_set_status_ready(st);
                return ec;
            }

            // If we received the final OK packet, we're no longer in a multi-function operation
            if (proc_->is_complete() && is_top_level_)
                st.status = connection_status::ready;
        }

        return next_action();
    }

    std::size_t result(const connection_state_data&) const { return state_.rows_read; }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
