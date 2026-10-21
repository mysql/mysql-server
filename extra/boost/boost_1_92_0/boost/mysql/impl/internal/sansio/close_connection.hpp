//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_INTERNAL_SANSIO_CLOSE_CONNECTION_HPP
#define BOOST_MYSQL_IMPL_INTERNAL_SANSIO_CLOSE_CONNECTION_HPP

#include <boost/mysql/diagnostics.hpp>

#include <boost/mysql/detail/algo_params.hpp>
#include <boost/mysql/detail/next_action.hpp>

#include <boost/mysql/impl/internal/coroutine.hpp>
#include <boost/mysql/impl/internal/sansio/connection_state_data.hpp>
#include <boost/mysql/impl/internal/sansio/quit_connection.hpp>

namespace boost {
namespace mysql {
namespace detail {

class close_connection_algo
{
    int resume_point_{0};
    quit_connection_algo quit_{{}};
    error_code stored_ec_;

public:
    close_connection_algo(close_connection_algo_params) noexcept {}

    next_action resume(connection_state_data& st, diagnostics& diag, error_code ec)
    {
        next_action act;

        switch (resume_point_)
        {
        case 0:

            // If we're not connected, we're done.
            // If we're in a multi-function operation, it's safe to proceed.
            if (st.status == connection_status::not_connected)
                return next_action();

            // Attempt quit
            while (!(act = quit_.resume(st, diag, ec)).is_done())
                BOOST_MYSQL_YIELD(resume_point_, 1, act)
            stored_ec_ = act.error();

            // Close the transport
            BOOST_MYSQL_YIELD(resume_point_, 2, next_action::close())

            // If quit resulted in an error, keep that error.
            // Otherwise, return any error derived from close
            if (stored_ec_)
                ec = stored_ec_;
        }

        return ec;
    }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
