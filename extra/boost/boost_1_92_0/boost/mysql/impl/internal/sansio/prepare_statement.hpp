//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_INTERNAL_SANSIO_PREPARE_STATEMENT_HPP
#define BOOST_MYSQL_IMPL_INTERNAL_SANSIO_PREPARE_STATEMENT_HPP

#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/statement.hpp>

#include <boost/mysql/detail/algo_params.hpp>
#include <boost/mysql/detail/next_action.hpp>

#include <boost/mysql/impl/internal/coroutine.hpp>
#include <boost/mysql/impl/internal/protocol/deserialization.hpp>
#include <boost/mysql/impl/internal/sansio/connection_state_data.hpp>

namespace boost {
namespace mysql {
namespace detail {

class read_prepare_statement_response_algo
{
    int resume_point_{0};
    std::uint8_t sequence_number_{0};
    unsigned remaining_meta_{0};
    statement res_;

    error_code process_response(connection_state_data& st, diagnostics& diag)
    {
        prepare_stmt_response response{};
        auto err = deserialize_prepare_stmt_response(st.reader.message(), st.flavor, response, diag);
        if (err)
            return err;
        res_ = access::construct<statement>(response.id, response.num_params);
        remaining_meta_ = response.num_columns + response.num_params;
        return error_code();
    }

public:
    read_prepare_statement_response_algo(std::uint8_t seqnum) noexcept : sequence_number_(seqnum) {}

    std::uint8_t& sequence_number() { return sequence_number_; }

    next_action resume(connection_state_data& st, diagnostics& diag, error_code ec)
    {
        if (ec)
            return ec;

        switch (resume_point_)
        {
        case 0:

            // Read response
            BOOST_MYSQL_YIELD(resume_point_, 1, st.read(sequence_number_))

            // Process response
            ec = process_response(st, diag);
            if (ec)
                return ec;

            // Server sends now one packet per parameter and field.
            // We ignore these for now.
            for (; remaining_meta_ > 0u; --remaining_meta_)
                BOOST_MYSQL_YIELD(resume_point_, 2, st.read(sequence_number_))
        }

        return next_action();
    }

    statement result(const connection_state_data&) const { return res_; }
};

class prepare_statement_algo
{
    int resume_point_{0};
    read_prepare_statement_response_algo read_response_st_;
    string_view stmt_sql_;

public:
    prepare_statement_algo(prepare_statement_algo_params params) noexcept
        : read_response_st_(0u), stmt_sql_(params.stmt_sql)
    {
    }

    next_action resume(connection_state_data& st, diagnostics& diag, error_code ec)
    {
        next_action act;

        switch (resume_point_)
        {
        case 0:
            // Check status
            ec = st.check_status_ready();
            if (ec)
                return ec;

            // Send request
            BOOST_MYSQL_YIELD(
                resume_point_,
                1,
                st.write(prepare_stmt_command{stmt_sql_}, read_response_st_.sequence_number())
            )
            if (ec)
                return ec;

            // Read response
            while (!(act = read_response_st_.resume(st, diag, ec)).is_done())
                BOOST_MYSQL_YIELD(resume_point_, 2, act)
            return act;
        }

        return next_action();
    }

    statement result(const connection_state_data& st) const { return read_response_st_.result(st); }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
