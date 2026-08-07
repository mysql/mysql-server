//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_IS_FATAL_ERROR_IPP
#define BOOST_MYSQL_IMPL_IS_FATAL_ERROR_IPP

#pragma once

#include <boost/mysql/client_errc.hpp>
#include <boost/mysql/common_server_errc.hpp>
#include <boost/mysql/error_categories.hpp>
#include <boost/mysql/is_fatal_error.hpp>

bool boost::mysql::is_fatal_error(error_code ec) noexcept
{
    // If there is no failure, it's not fatal
    if (!ec)
        return false;

    // Retrieve the error category
    const auto& cat = ec.category();

    if (cat == get_common_server_category())
    {
        // Server errors may or may not be fatal. MySQL defines a ton of different errors.
        // After some research, these are the ones I'd recommend to consider fatal
        auto code = static_cast<common_server_errc>(ec.value());
        switch (code)
        {
        // Different flavors of communication errors. These usually indicate that the connection
        // has been left in an unspecified state, and the safest is to reconnect it.
        case common_server_errc::er_unknown_com_error:
        case common_server_errc::er_aborting_connection:
        case common_server_errc::er_net_packet_too_large:
        case common_server_errc::er_net_read_error_from_pipe:
        case common_server_errc::er_net_fcntl_error:
        case common_server_errc::er_net_packets_out_of_order:
        case common_server_errc::er_net_uncompress_error:
        case common_server_errc::er_net_read_error:
        case common_server_errc::er_net_read_interrupted:
        case common_server_errc::er_net_error_on_write:
        case common_server_errc::er_net_write_interrupted:
        case common_server_errc::er_malformed_packet:
        case common_server_errc::er_zlib_z_buf_error:
        case common_server_errc::er_zlib_z_data_error:
        case common_server_errc::er_zlib_z_mem_error: return true;
        default: return false;
        }
    }
    else if (cat == get_mysql_server_category() || cat == get_mariadb_server_category())
    {
        // DB-specific codes are all non fatal
        return false;
    }
    else if (cat == get_client_category())
    {
        auto code = static_cast<client_errc>(ec.value());
        switch (code)
        {
        // These indicate malformed frames or packet mismatches
        case client_errc::incomplete_message:
        case client_errc::protocol_value_error:
        case client_errc::extra_bytes:
        case client_errc::sequence_number_mismatch:

        // Exceeding the max buffer size is not recoverable
        case client_errc::max_buffer_size_exceeded:

        // These are produced by the static interface, and currently cause parsing
        // to stop, leaving unread packets in the network buffer.
        // See https://github.com/boostorg/mysql/issues/212
        case client_errc::metadata_check_failed:
        case client_errc::num_resultsets_mismatch:
        case client_errc::row_type_mismatch:
        case client_errc::static_row_parsing_error:

        // These are only produced by handshake. We categorize them as fatal because they need reconnection,
        // although anything affecting handshake effectively does.
        case client_errc::server_doesnt_support_ssl:
        case client_errc::unknown_auth_plugin:
        case client_errc::server_unsupported:
        case client_errc::auth_plugin_requires_ssl:
        case client_errc::bad_handshake_packet_type:
        case client_errc::unknown_openssl_error: return true;

        default: return false;
        }
    }
    else
    {
        // Other categories are fatal - these include network and SSL errors
        return true;
    }
}

#endif
