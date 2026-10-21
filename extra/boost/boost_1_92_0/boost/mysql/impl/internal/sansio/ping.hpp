//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_INTERNAL_SANSIO_PING_HPP
#define BOOST_MYSQL_IMPL_INTERNAL_SANSIO_PING_HPP

#include <boost/mysql/diagnostics.hpp>

#include <boost/mysql/detail/algo_params.hpp>

#include <boost/mysql/impl/internal/coroutine.hpp>
#include <boost/mysql/impl/internal/protocol/serialization.hpp>
#include <boost/mysql/impl/internal/sansio/connection_state_data.hpp>

namespace boost {
namespace mysql {
namespace detail {

class read_ping_response_algo
{
    int resume_point_{0};
    std::uint8_t seqnum_{0};

public:
    read_ping_response_algo(std::uint8_t seqnum) noexcept : seqnum_(seqnum) {}

    next_action resume(connection_state_data& st, diagnostics& diag, error_code ec)
    {
        switch (resume_point_)
        {
        case 0:

            // Issue a read
            BOOST_MYSQL_YIELD(resume_point_, 1, st.read(seqnum_))
            if (ec)
                return ec;

            // Process the OK packet and done
            ec = st.deserialize_ok(diag);
        }

        return ec;
    }
};

inline run_pipeline_algo_params setup_ping_pipeline(connection_state_data& st)
{
    // The ping request is fixed size and small. No buffer limit is enforced on it.
    st.write_buffer.clear();
    st.shared_pipeline_stages[0] = {
        pipeline_stage_kind::ping,
        serialize_top_level_checked(ping_command{}, st.write_buffer),
        {}
    };
    return {
        st.write_buffer,
        {st.shared_pipeline_stages.data(), 1},
        nullptr
    };
}

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
