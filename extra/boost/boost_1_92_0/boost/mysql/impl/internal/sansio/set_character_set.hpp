//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_INTERNAL_SANSIO_SET_CHARACTER_SET_HPP
#define BOOST_MYSQL_IMPL_INTERNAL_SANSIO_SET_CHARACTER_SET_HPP

#include <boost/mysql/character_set.hpp>
#include <boost/mysql/client_errc.hpp>
#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/format_sql.hpp>

#include <boost/mysql/detail/algo_params.hpp>
#include <boost/mysql/detail/next_action.hpp>

#include <boost/mysql/impl/internal/coroutine.hpp>
#include <boost/mysql/impl/internal/protocol/deserialization.hpp>
#include <boost/mysql/impl/internal/protocol/serialization.hpp>
#include <boost/mysql/impl/internal/sansio/connection_state_data.hpp>

#include <boost/system/result.hpp>

#include <cstdint>

namespace boost {
namespace mysql {
namespace detail {

// Securely compose a SET NAMES statement
inline system::result<std::string> compose_set_names(character_set charset)
{
    // The character set should not be default-constructed
    BOOST_ASSERT(charset.name != nullptr);

    // For security, if the character set has non-ascii characters in it name, reject it.
    format_context ctx(format_options{ascii_charset, true});
    ctx.append_raw("SET NAMES ").append_value(charset.name);
    return std::move(ctx).get();
}

class read_set_character_set_response_algo
{
    int resume_point_{0};
    character_set charset_;
    std::uint8_t seqnum_{0};

public:
    read_set_character_set_response_algo(character_set charset, std::uint8_t seqnum)
        : charset_(charset), seqnum_(seqnum)
    {
    }
    character_set charset() const { return charset_; }
    std::uint8_t& sequence_number() { return seqnum_; }

    next_action resume(connection_state_data& st, diagnostics& diag, error_code ec)
    {
        // SET NAMES never returns rows. Using execute requires us to allocate
        // a results object, which we can avoid by simply sending the query and reading the OK response.
        switch (resume_point_)
        {
        case 0:

            // Read the response
            BOOST_MYSQL_YIELD(resume_point_, 1, st.read(seqnum_))
            if (ec)
                return ec;

            // Verify it's what we expected
            ec = st.deserialize_ok(diag);
            if (ec)
                return ec;

            // If we were successful, update the character set
            st.current_charset = charset_;
        }

        return next_action();
    }
};

class set_character_set_algo
{
    int resume_point_{0};
    read_set_character_set_response_algo read_response_st_;

    next_action compose_request(connection_state_data& st)
    {
        auto q = compose_set_names(read_response_st_.charset());
        if (q.has_error())
            return q.error();
        return st.write(query_command{q.value()}, read_response_st_.sequence_number());
    }

public:
    set_character_set_algo(set_character_set_algo_params params) noexcept
        : read_response_st_(params.charset, 0u)
    {
    }

    next_action resume(connection_state_data& st, diagnostics& diag, error_code ec)
    {
        next_action act;

        // SET NAMES never returns rows. Using execute requires us to allocate
        // a results object, which we can avoid by simply sending the query and reading the OK response.
        switch (resume_point_)
        {
        case 0:

            // Check status
            ec = st.check_status_ready();
            if (ec)
                return ec;

            // Send the execution request
            BOOST_MYSQL_YIELD(resume_point_, 1, compose_request(st))
            if (ec)
                return ec;

            // Read the response
            while (!(act = read_response_st_.resume(st, diag, ec)).is_done())
                BOOST_MYSQL_YIELD(resume_point_, 2, act)
            return act;
        }

        return next_action();
    }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
