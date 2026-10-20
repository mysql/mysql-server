//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_INTERNAL_SANSIO_CONNECTION_STATE_DATA_HPP
#define BOOST_MYSQL_IMPL_INTERNAL_SANSIO_CONNECTION_STATE_DATA_HPP

#include <boost/mysql/character_set.hpp>
#include <boost/mysql/client_errc.hpp>
#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/field_view.hpp>
#include <boost/mysql/metadata_mode.hpp>

#include <boost/mysql/detail/next_action.hpp>
#include <boost/mysql/detail/pipeline.hpp>

#include <boost/mysql/impl/internal/protocol/capabilities.hpp>
#include <boost/mysql/impl/internal/protocol/db_flavor.hpp>
#include <boost/mysql/impl/internal/protocol/serialization.hpp>
#include <boost/mysql/impl/internal/sansio/message_reader.hpp>

#include <boost/assert.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace boost {
namespace mysql {
namespace detail {

enum class connection_status
{
    // Never connected or closed
    not_connected,

    // Connected and ready for a command
    ready,

    // In the middle of a multi-function operation
    engaged_in_multi_function,
};

struct connection_state_data
{
    // Are we connected? In the middle of a multi-function operation?
    connection_status status{connection_status::not_connected};

    // Are we currently executing an operation?
    // Prevent the user from running concurrent operations
    bool op_in_progress{false};

    // Are we talking to MySQL or MariaDB?
    db_flavor flavor{db_flavor::mysql};

    // What are the connection's capabilities?
    capabilities current_capabilities{};

    // The current connection ID. Supplied by handshake, can be used in KILL statements
    std::uint32_t connection_id{};

    // Used by async ops without output diagnostics params, to avoid allocations
    diagnostics shared_diag;

    // Temporary field storage, re-used by several ops
    std::vector<field_view> shared_fields;

    // Temporary pipeline stage storage, re-used by several ops
    std::array<pipeline_request_stage, 2> shared_pipeline_stages;

    // Do we want to retain metadata strings or not? Used to save allocations
    metadata_mode meta_mode{metadata_mode::minimal};

    // Is TLS supported for the current connection?
    bool tls_supported;

    // Is TLS enabled for the current connection?
    bool tls_active{false};

    // Do backslashes represent escape sequences? By default they do, but they can
    // be disabled using a variable. OK packets include a flag with this info.
    bool backslash_escapes{true};

    // The server's default character set, or a value-initialized one if unknown.
    // This may be passed by the user as an optimization.
    character_set server_default_charset{};

    // The current character set, or a value-initialized one if unknown
    character_set current_charset{};

    // The write buffer
    std::vector<std::uint8_t> write_buffer;

    // Reader
    message_reader reader;

    std::size_t max_buffer_size() const { return reader.max_buffer_size(); }

    connection_state_data(
        std::size_t read_buffer_size,
        std::size_t max_buff_size = static_cast<std::size_t>(-1),
        bool transport_supports_ssl = false
    )
        : tls_supported(transport_supports_ssl), reader(read_buffer_size, max_buff_size)
    {
    }

    void reset()
    {
        status = connection_status::not_connected;
        flavor = db_flavor::mysql;
        current_capabilities = capabilities{};
        connection_id = 0u;
        // Metadata mode does not get reset on handshake
        reader.reset();
        // Writer does not need reset, since every write clears previous state
        tls_active = false;
        backslash_escapes = true;
        current_charset = character_set{};
        server_default_charset = character_set{};
    }

    // Reads an OK packet from the reader. This operation is repeated in several places.
    error_code deserialize_ok(diagnostics& diag)
    {
        return deserialize_ok_response(reader.message(), flavor, diag, backslash_escapes);
    }

    // Helpers for sans-io algorithms
    next_action read(std::uint8_t& seqnum, bool keep_parsing_state = false)
    {
        // buffer is attached by top_level_algo
        reader.prepare_read(seqnum, keep_parsing_state);
        return next_action::read({});
    }

    template <class Serializable>
    next_action write(const Serializable& msg, std::uint8_t& seqnum)
    {
        // use_ssl is attached by top_level_algo
        write_buffer.clear();
        auto res = serialize_top_level(msg, write_buffer, seqnum, max_buffer_size());
        if (res.err)
            return res.err;
        seqnum = res.seqnum;
        return next_action::write({write_buffer, false});
    }

    // Helpers to implement connection status in algorithms
    error_code check_status_ready() const
    {
        switch (status)
        {
        case connection_status::not_connected: return client_errc::not_connected;
        case connection_status::engaged_in_multi_function: return client_errc::engaged_in_multi_function;
        default: BOOST_ASSERT(status == connection_status::ready); return error_code();
        }
    }

    error_code check_status_multi_function() const
    {
        switch (status)
        {
        case connection_status::not_connected: return client_errc::not_connected;
        case connection_status::ready: return client_errc::not_engaged_in_multi_function;
        default: BOOST_ASSERT(status == connection_status::engaged_in_multi_function); return error_code();
        }
    }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
