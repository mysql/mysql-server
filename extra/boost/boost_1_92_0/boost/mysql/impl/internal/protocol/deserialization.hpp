//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_INTERNAL_PROTOCOL_DESERIALIZATION_HPP
#define BOOST_MYSQL_IMPL_INTERNAL_PROTOCOL_DESERIALIZATION_HPP

#include <boost/mysql/client_errc.hpp>
#include <boost/mysql/column_type.hpp>
#include <boost/mysql/common_server_errc.hpp>
#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_categories.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/field_kind.hpp>
#include <boost/mysql/field_view.hpp>
#include <boost/mysql/metadata_collection_view.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/mysql/detail/coldef_view.hpp>
#include <boost/mysql/detail/config.hpp>
#include <boost/mysql/detail/ok_view.hpp>
#include <boost/mysql/detail/resultset_encoding.hpp>

#include <boost/mysql/impl/internal/error/server_error_to_string.hpp>
#include <boost/mysql/impl/internal/protocol/capabilities.hpp>
#include <boost/mysql/impl/internal/protocol/db_flavor.hpp>
#include <boost/mysql/impl/internal/protocol/impl/binary_protocol.hpp>
#include <boost/mysql/impl/internal/protocol/impl/deserialization_context.hpp>
#include <boost/mysql/impl/internal/protocol/impl/null_bitmap.hpp>
#include <boost/mysql/impl/internal/protocol/impl/protocol_field_type.hpp>
#include <boost/mysql/impl/internal/protocol/impl/text_protocol.hpp>
#include <boost/mysql/impl/internal/protocol/static_buffer.hpp>

#include <boost/config.hpp>
#include <boost/core/ignore_unused.hpp>
#include <boost/core/span.hpp>

#include <cstddef>
#include <cstdint>

namespace boost {
namespace mysql {
namespace detail {

// OK packets (views because strings are non-owning)
inline error_code deserialize_ok_packet(span<const std::uint8_t> msg, ok_view& output);  // for testing

// Error packets (exposed for testing)
struct err_view
{
    std::uint16_t error_code;
    string_view error_message;
};
BOOST_ATTRIBUTE_NODISCARD inline error_code deserialize_error_packet(
    span<const std::uint8_t> message,
    err_view& pack,
    bool has_sql_state = true
);
BOOST_ATTRIBUTE_NODISCARD inline error_code process_error_packet(
    span<const std::uint8_t> message,
    db_flavor flavor,
    diagnostics& diag,
    bool has_sql_state = true
);

// Deserializes a response that may be an OK or an error packet.
// Applicable for commands like ping and reset connection.
// If the response is an OK packet, sets backslash_escapes according to the
// OK packet's server status flags
BOOST_ATTRIBUTE_NODISCARD inline error_code deserialize_ok_response(
    span<const std::uint8_t> message,
    db_flavor flavor,
    diagnostics& diag,
    bool& backslash_escapes
);

// Column definition
BOOST_ATTRIBUTE_NODISCARD inline error_code deserialize_column_definition(
    span<const std::uint8_t> input,
    coldef_view& output
);

// Prepare statement response
struct prepare_stmt_response
{
    std::uint32_t id;
    std::uint16_t num_columns;
    std::uint16_t num_params;
};
BOOST_ATTRIBUTE_NODISCARD inline error_code deserialize_prepare_stmt_response_impl(
    span<const std::uint8_t> message,
    prepare_stmt_response& output
);  // exposed for testing, doesn't take header into account
BOOST_ATTRIBUTE_NODISCARD inline error_code deserialize_prepare_stmt_response(
    span<const std::uint8_t> message,
    db_flavor flavor,
    prepare_stmt_response& output,
    diagnostics& diag
);

// Execution messages
struct execute_response
{
    enum class type_t
    {
        num_fields,
        ok_packet,
        error
    } type;
    union data_t
    {
        std::size_t num_fields;
        ok_view ok_pack;
        error_code err;

        data_t(size_t v) noexcept : num_fields(v) {}
        data_t(const ok_view& v) noexcept : ok_pack(v) {}
        data_t(error_code v) noexcept : err(v) {}
    } data;

    execute_response(std::size_t v) noexcept : type(type_t::num_fields), data(v) {}
    execute_response(const ok_view& v) noexcept : type(type_t::ok_packet), data(v) {}
    execute_response(error_code v) noexcept : type(type_t::error), data(v) {}
};
inline execute_response deserialize_execute_response(
    span<const std::uint8_t> msg,
    db_flavor flavor,
    diagnostics& diag
);

struct row_message
{
    enum class type_t
    {
        row,
        ok_packet,
        error
    } type;
    union data_t
    {
        span<const std::uint8_t> row;
        ok_view ok_pack;
        error_code err;

        data_t(span<const std::uint8_t> row) noexcept : row(row) {}
        data_t(const ok_view& ok_pack) noexcept : ok_pack(ok_pack) {}
        data_t(error_code err) noexcept : err(err) {}
    } data;

    row_message(span<const std::uint8_t> row) noexcept : type(type_t::row), data(row) {}
    row_message(const ok_view& ok_pack) noexcept : type(type_t::ok_packet), data(ok_pack) {}
    row_message(error_code v) noexcept : type(type_t::error), data(v) {}
};
inline row_message deserialize_row_message(span<const std::uint8_t> msg, db_flavor flavor, diagnostics& diag);

inline error_code deserialize_row(
    resultset_encoding encoding,
    span<const std::uint8_t> message,
    metadata_collection_view meta,
    span<field_view> output  // Should point to meta.size() field_view objects
);

// Server hello
struct server_hello
{
    using auth_buffer_type = static_buffer<8 + 0xff>;
    db_flavor server;
    std::uint32_t connection_id{};
    auth_buffer_type auth_plugin_data;
    capabilities server_capabilities{};
    string_view auth_plugin_name;
};
BOOST_ATTRIBUTE_NODISCARD inline error_code deserialize_server_hello_impl(
    span<const std::uint8_t> msg,
    server_hello& output
);  // exposed for testing, doesn't take message header into account
BOOST_ATTRIBUTE_NODISCARD inline error_code deserialize_server_hello(
    span<const std::uint8_t> msg,
    server_hello& output,
    diagnostics& diag
);

// Auth switch
struct auth_switch
{
    string_view plugin_name;
    span<const std::uint8_t> auth_data;
};
BOOST_ATTRIBUTE_NODISCARD inline error_code deserialize_auth_switch(
    span<const std::uint8_t> msg,
    auth_switch& output
);  // exposed for testing

// Handshake server response
struct handshake_server_response
{
    enum class type_t
    {
        ok,
        error,
        auth_switch,
        auth_more_data
    } type;

    union data_t
    {
        ok_view ok;
        error_code err;
        auth_switch auth_sw;
        span<const std::uint8_t> more_data;

        data_t(const ok_view& ok) noexcept : ok(ok) {}
        data_t(error_code err) noexcept : err(err) {}
        data_t(auth_switch msg) noexcept : auth_sw(msg) {}
        data_t(span<const std::uint8_t> more_data) noexcept : more_data(more_data) {}
    } data;

    handshake_server_response(const ok_view& ok) noexcept : type(type_t::ok), data(ok) {}
    handshake_server_response(error_code err) noexcept : type(type_t::error), data(err) {}
    handshake_server_response(auth_switch auth_switch) noexcept : type(type_t::auth_switch), data(auth_switch)
    {
    }
    handshake_server_response(span<const std::uint8_t> more_data) noexcept
        : type(type_t::auth_more_data), data(more_data)
    {
    }
};
inline handshake_server_response deserialize_handshake_server_response(
    span<const std::uint8_t> buff,
    db_flavor flavor,
    diagnostics& diag
);

}  // namespace detail
}  // namespace mysql
}  // namespace boost

//
// Implementations
//

namespace boost {
namespace mysql {
namespace detail {

// Constants
BOOST_INLINE_CONSTEXPR std::uint8_t error_packet_header = 0xff;
BOOST_INLINE_CONSTEXPR std::uint8_t ok_packet_header = 0x00;

}  // namespace detail
}  // namespace mysql
}  // namespace boost

//
// Deserialization
//

// OK packets
boost::mysql::error_code boost::mysql::detail::deserialize_ok_packet(
    span<const std::uint8_t> msg,
    ok_view& output
)
{
    struct ok_packet
    {
        // header: int<1>     header     0x00 or 0xFE the OK packet header
        int_lenenc affected_rows;
        int_lenenc last_insert_id;
        int2 status_flags;  // server_status_flags
        int2 warnings;
        // CLIENT_SESSION_TRACK: not implemented
        string_lenenc info;
    } pack{};

    deserialization_context ctx(msg);
    auto err = ctx.deserialize(pack.affected_rows, pack.last_insert_id, pack.status_flags, pack.warnings);
    if (err != deserialize_errc::ok)
        return to_error_code(err);

    if (ctx.enough_size(1))  // message is optional, may be omitted
    {
        err = pack.info.deserialize(ctx);
        if (err != deserialize_errc::ok)
            return to_error_code(err);
    }

    output = {
        pack.affected_rows.value,
        pack.last_insert_id.value,
        pack.status_flags.value,
        pack.warnings.value,
        pack.info.value,
    };

    return ctx.check_extra_bytes();
}

// Error packets
boost::mysql::error_code boost::mysql::detail::deserialize_error_packet(
    span<const std::uint8_t> msg,
    err_view& output,
    bool has_sql_state
)
{
    struct err_packet
    {
        // int1     header     0xFF ERR packet header
        int2 error_code;
        // if capabilities & CLIENT_PROTOCOL_41 {  (modeled here as has_sql_state)
        string_fixed<1> sql_state_marker;
        string_fixed<5> sql_state;
        // }
        string_eof error_message;
    } pack{};

    deserialization_context ctx(msg);
    auto err = has_sql_state ? ctx.deserialize(
                                   pack.error_code,
                                   pack.sql_state_marker,
                                   pack.sql_state,
                                   pack.error_message
                               )
                             : ctx.deserialize(pack.error_code, pack.error_message);
    if (err != deserialize_errc::ok)
        return to_error_code(err);

    output = err_view{
        pack.error_code.value,
        pack.error_message.value,
    };

    return ctx.check_extra_bytes();
}

boost::mysql::error_code boost::mysql::detail::process_error_packet(
    span<const std::uint8_t> msg,
    db_flavor flavor,
    diagnostics& diag,
    bool has_sql_state
)
{
    err_view error_packet{};
    auto err = deserialize_error_packet(msg, error_packet, has_sql_state);
    if (err)
        return err;

    // Error message
    access::get_impl(diag).assign_server(error_packet.error_message);

    // Error code
    if (common_error_to_string(error_packet.error_code))
    {
        // This is an error shared between MySQL and MariaDB, represented as a common_server_errc.
        // get_common_error_message will check that the code has a common_server_errc representation
        // (the common error range has "holes" because of removed error codes)
        return static_cast<common_server_errc>(error_packet.error_code);
    }
    else
    {
        // This is a MySQL or MariaDB specific code. There is no fixed list of error codes,
        // as they both keep adding more codes, so no validation happens.
        const auto& cat = flavor == db_flavor::mysql ? get_mysql_server_category()
                                                     : get_mariadb_server_category();
        return error_code(error_packet.error_code, cat);
    }
}

// Column definition
boost::mysql::error_code boost::mysql::detail::deserialize_column_definition(
    span<const std::uint8_t> input,
    coldef_view& output
)
{
    deserialization_context ctx(input);

    struct column_definition_packet
    {
        string_lenenc catalog;    // always "def"
        string_lenenc schema;     // database
        string_lenenc table;      // virtual table
        string_lenenc org_table;  // physical table
        string_lenenc name;       // virtual column name
        string_lenenc org_name;   // physical column name
        string_lenenc fixed_fields;
    } pack{};

    // pack.fixed_fields itself is a structure like this.
    // The proto allows for extensibility here - adding fields just increasing fixed_fields.length
    struct fixed_fields_pack
    {
        int2 character_set;  // collation id, somehow named character_set in the protocol docs
        int4 column_length;  // maximum length of the field
        int1 type;      // type of the column as defined in enum_field_types - this is a protocol_field_type
        int2 flags;     // Flags as defined in Column Definition Flags
        int1 decimals;  // max shown decimal digits. 0x00 for int/static strings; 0x1f for
                        // dynamic strings, double, float
    } fixed_fields{};

    // Deserialize the main structure
    auto err = ctx.deserialize(
        pack.catalog,
        pack.schema,
        pack.table,
        pack.org_table,
        pack.name,
        pack.org_name,
        pack.fixed_fields
    );
    if (err != deserialize_errc::ok)
        return to_error_code(err);

    // Deserialize the fixed_fields structure.
    // Intentionally not checking for extra bytes here, since there may be unknown fields that should just get
    // ignored
    deserialization_context subctx(to_span(pack.fixed_fields.value));
    err = subctx.deserialize(
        fixed_fields.character_set,
        fixed_fields.column_length,
        fixed_fields.type,
        fixed_fields.flags,
        fixed_fields.decimals
    );
    if (err != deserialize_errc::ok)
        return to_error_code(err);

    // Compose output
    output = coldef_view{
        pack.schema.value,
        pack.table.value,
        pack.org_table.value,
        pack.name.value,
        pack.org_name.value,
        fixed_fields.character_set.value,
        fixed_fields.column_length.value,
        compute_column_type(
            static_cast<protocol_field_type>(fixed_fields.type.value),
            fixed_fields.flags.value,
            fixed_fields.character_set.value
        ),
        fixed_fields.flags.value,
        fixed_fields.decimals.value,
    };

    return ctx.check_extra_bytes();
}

boost::mysql::error_code boost::mysql::detail::deserialize_ok_response(
    span<const std::uint8_t> message,
    db_flavor flavor,
    diagnostics& diag,
    bool& backslash_escapes
)
{
    // Header
    int1 header{};
    deserialization_context ctx(message);
    auto err = to_error_code(header.deserialize(ctx));
    if (err)
        return err;

    if (header.value == ok_packet_header)
    {
        // Verify that the ok_packet is correct
        ok_view ok{};
        err = deserialize_ok_packet(ctx.to_span(), ok);
        if (err)
            return err;
        backslash_escapes = ok.backslash_escapes();
        return error_code();
    }
    else if (header.value == error_packet_header)
    {
        // Theoretically, the server can answer with an error packet, too
        return process_error_packet(ctx.to_span(), flavor, diag);
    }
    else
    {
        // Invalid message
        return client_errc::protocol_value_error;
    }
}

boost::mysql::error_code boost::mysql::detail::deserialize_prepare_stmt_response_impl(
    span<const std::uint8_t> message,
    prepare_stmt_response& output
)
{
    struct com_stmt_prepare_ok_packet
    {
        // std::uint8_t status: must be 0
        int4 statement_id;
        int2 num_columns;
        int2 num_params;
        int1 reserved_1;  // must be 0
        int2 warning_count;
        // int1 metadata_follows when CLIENT_OPTIONAL_RESULTSET_METADATA: not implemented
    } pack{};

    deserialization_context ctx(message);

    auto err = ctx.deserialize(
        pack.statement_id,
        pack.num_columns,
        pack.num_params,
        pack.reserved_1,
        pack.warning_count
    );
    if (err != deserialize_errc::ok)
        return to_error_code(err);

    output = prepare_stmt_response{
        pack.statement_id.value,
        pack.num_columns.value,
        pack.num_params.value,
    };

    return ctx.check_extra_bytes();
}

boost::mysql::error_code boost::mysql::detail::deserialize_prepare_stmt_response(
    span<const std::uint8_t> message,
    db_flavor flavor,
    prepare_stmt_response& output,
    diagnostics& diag
)
{
    deserialization_context ctx(message);
    int1 msg_type{};
    auto err = to_error_code(msg_type.deserialize(ctx));
    if (err)
        return err;

    if (msg_type.value == error_packet_header)
    {
        return process_error_packet(ctx.to_span(), flavor, diag);
    }
    else if (msg_type.value != 0)
    {
        return client_errc::protocol_value_error;
    }
    else
    {
        return deserialize_prepare_stmt_response_impl(ctx.to_span(), output);
    }
}

// execute response
boost::mysql::detail::execute_response boost::mysql::detail::deserialize_execute_response(
    span<const std::uint8_t> msg,
    db_flavor flavor,
    diagnostics& diag
)
{
    // Response may be: ok_packet, err_packet, local infile request (not implemented)
    // If it is none of this, then the message type itself is the beginning of
    // a length-encoded int containing the field count
    deserialization_context ctx(msg);
    int1 msg_type{};
    auto err = to_error_code(msg_type.deserialize(ctx));
    if (err)
        return err;

    if (msg_type.value == ok_packet_header)
    {
        ok_view ok{};
        err = deserialize_ok_packet(ctx.to_span(), ok);
        if (err)
            return err;
        return ok;
    }
    else if (msg_type.value == error_packet_header)
    {
        return process_error_packet(ctx.to_span(), flavor, diag);
    }
    else
    {
        // Resultset with metadata. First packet is an int_lenenc with
        // the number of field definitions to expect. Message type is part
        // of this packet, so we must rewind the context
        ctx.rewind(1);
        int_lenenc num_fields{};
        err = to_error_code(num_fields.deserialize(ctx));
        if (err)
            return err;
        err = ctx.check_extra_bytes();
        if (err)
            return err;

        // We should have at least one field.
        // The max number of fields is some value around 1024. For simplicity/extensibility,
        // we accept anything less than 0xffff
        if (num_fields.value == 0 || num_fields.value > 0xffffu)
        {
            return make_error_code(client_errc::protocol_value_error);
        }

        return static_cast<std::size_t>(num_fields.value);
    }
}

boost::mysql::detail::row_message boost::mysql::detail::deserialize_row_message(
    span<const std::uint8_t> msg,
    db_flavor flavor,
    diagnostics& diag
)
{
    constexpr std::uint8_t eof_packet_header = 0xfe;

    // Message type: row, error or eof?
    int1 msg_type{};
    deserialization_context ctx(msg);
    auto deser_errc = msg_type.deserialize(ctx);
    if (deser_errc != deserialize_errc::ok)
    {
        return to_error_code(deser_errc);
    }

    if (msg_type.value == eof_packet_header)
    {
        // end of resultset => this is a ok_packet, not a row
        ok_view ok{};
        auto err = deserialize_ok_packet(ctx.to_span(), ok);
        if (err)
            return err;
        return ok;
    }
    else if (msg_type.value == error_packet_header)
    {
        // An error occurred during the generation of the rows
        return process_error_packet(ctx.to_span(), flavor, diag);
    }
    else
    {
        // An actual row
        ctx.rewind(1);  // keep the 'message type' byte, as it is part of the actual message
        return span<const std::uint8_t>(ctx.first(), ctx.size());
    }
}

// Deserialize row
namespace boost {
namespace mysql {
namespace detail {

inline bool is_next_field_null(const deserialization_context& ctx)
{
    if (!ctx.enough_size(1))
        return false;
    return *ctx.first() == 0xfb;
}

inline error_code deserialize_text_row(
    deserialization_context& ctx,
    metadata_collection_view meta,
    field_view* output
)
{
    for (std::vector<field_view>::size_type i = 0; i < meta.size(); ++i)
    {
        if (is_next_field_null(ctx))
        {
            ctx.advance(1);
            output[i] = field_view(nullptr);
        }
        else
        {
            string_lenenc value_str;
            auto err = value_str.deserialize(ctx);
            if (err != deserialize_errc::ok)
                return to_error_code(err);
            err = deserialize_text_field(value_str.value, meta[i], output[i]);
            if (err != deserialize_errc::ok)
                return to_error_code(err);
        }
    }
    return ctx.check_extra_bytes();
}

inline error_code deserialize_binary_row(
    deserialization_context& ctx,
    metadata_collection_view meta,
    field_view* output
)
{
    // Skip packet header (it is not part of the message in the binary
    // protocol but it is in the text protocol, so we include it for homogeneity)
    if (!ctx.enough_size(1))
        return client_errc::incomplete_message;
    ctx.advance(1);

    // Number of fields
    std::size_t num_fields = meta.size();

    // Null bitmap
    null_bitmap_parser null_bitmap(num_fields);
    const std::uint8_t* null_bitmap_first = ctx.first();
    std::size_t null_bitmap_size = null_bitmap.byte_count();
    if (!ctx.enough_size(null_bitmap_size))
        return client_errc::incomplete_message;
    ctx.advance(null_bitmap_size);

    // Actual values
    for (std::vector<field_view>::size_type i = 0; i < num_fields; ++i)
    {
        if (null_bitmap.is_null(null_bitmap_first, i))
        {
            output[i] = field_view(nullptr);
        }
        else
        {
            auto err = deserialize_binary_field(ctx, meta[i], output[i]);
            if (err != deserialize_errc::ok)
                return to_error_code(err);
        }
    }

    // Check for remaining bytes
    return ctx.check_extra_bytes();
}

}  // namespace detail
}  // namespace mysql
}  // namespace boost

boost::mysql::error_code boost::mysql::detail::deserialize_row(
    resultset_encoding encoding,
    span<const std::uint8_t> buff,
    metadata_collection_view meta,
    span<field_view> output
)
{
    BOOST_ASSERT(meta.size() == output.size());
    deserialization_context ctx(buff);
    return encoding == detail::resultset_encoding::text ? deserialize_text_row(ctx, meta, output.data())
                                                        : deserialize_binary_row(ctx, meta, output.data());
}

// Server hello
namespace boost {
namespace mysql {
namespace detail {

inline capabilities compose_capabilities(string_fixed<2> low, string_fixed<2> high)
{
    std::uint32_t res = 0;
    auto capabilities_begin = reinterpret_cast<std::uint8_t*>(&res);
    memcpy(capabilities_begin, low.value.data(), 2);
    memcpy(capabilities_begin + 2, high.value.data(), 2);
    return static_cast<capabilities>(boost::endian::little_to_native(res));
}

inline db_flavor parse_db_version(string_view version_string)
{
    return version_string.find("MariaDB") != string_view::npos ? db_flavor::mariadb : db_flavor::mysql;
}

}  // namespace detail
}  // namespace mysql
}  // namespace boost

boost::mysql::error_code boost::mysql::detail::deserialize_server_hello_impl(
    span<const std::uint8_t> msg,
    server_hello& output
)
{
    struct server_hello_packet
    {
        // int<1>     protocol version     Always 10
        string_null server_version;
        int4 connection_id;
        string_fixed<8> auth_plugin_data_part_1;
        int1 filler;  // should be 0
        string_fixed<2> capability_flags_low;
        int1 character_set;  // default server a_protocol_character_set, only the lower 8-bits
        int2 status_flags;   // server_status_flags
        string_fixed<2> capability_flags_high;
        int1 auth_plugin_data_len;
        string_fixed<10> reserved;
        // auth plugin data, 2nd part. This has a weird representation that doesn't fit any defined type
        string_null auth_plugin_name;
    } pack{};

    deserialization_context ctx(msg);

    auto err = ctx.deserialize(
        pack.server_version,
        pack.connection_id,
        pack.auth_plugin_data_part_1,
        pack.filler,
        pack.capability_flags_low,
        pack.character_set,
        pack.status_flags,
        pack.capability_flags_high
    );
    if (err != deserialize_errc::ok)
        return to_error_code(err);

    // Compose capabilities
    auto cap = compose_capabilities(pack.capability_flags_low, pack.capability_flags_high);

    // Check minimum server capabilities to deserialize this frame
    if (!has_capabilities(cap, capabilities::plugin_auth))
        return client_errc::server_unsupported;

    // Deserialize next fields
    err = ctx.deserialize(pack.auth_plugin_data_len, pack.reserved);
    if (err != deserialize_errc::ok)
        return to_error_code(err);

    // Auth plugin data, second part
    auto auth2_length = static_cast<std::uint8_t>((std::max)(
        static_cast<std::size_t>(13u),
        static_cast<std::size_t>(pack.auth_plugin_data_len.value - pack.auth_plugin_data_part_1.value.size())
    ));
    const std::uint8_t* auth2_data = ctx.first();
    if (!ctx.enough_size(auth2_length))
        return client_errc::incomplete_message;
    ctx.advance(auth2_length);

    // Auth plugin name
    err = pack.auth_plugin_name.deserialize(ctx);
    if (err != deserialize_errc::ok)
        return to_error_code(err);

    // Compose output
    output.connection_id = pack.connection_id.value;
    output.server = parse_db_version(pack.server_version.value);
    output.server_capabilities = cap;
    output.auth_plugin_name = pack.auth_plugin_name.value;

    // Compose auth_plugin_data
    output.auth_plugin_data.clear();
    output.auth_plugin_data.append(span<const std::uint8_t>(
        reinterpret_cast<const std::uint8_t*>(pack.auth_plugin_data_part_1.value.data()),
        pack.auth_plugin_data_part_1.value.size()
    ));

    // discard an extra trailing NULL byte
    output.auth_plugin_data.append(span<const std::uint8_t>(auth2_data, auth2_length - 1));

    return ctx.check_extra_bytes();
}

boost::mysql::error_code boost::mysql::detail::deserialize_server_hello(
    span<const std::uint8_t> msg,
    server_hello& output,
    diagnostics& diag
)
{
    constexpr std::uint8_t handshake_protocol_version_9 = 9;
    constexpr std::uint8_t handshake_protocol_version_10 = 10;

    deserialization_context ctx(msg);

    // Message type
    int1 msg_type{};
    auto err = to_error_code(msg_type.deserialize(ctx));
    if (err)
        return err;
    if (msg_type.value == handshake_protocol_version_9)
    {
        return make_error_code(client_errc::server_unsupported);
    }
    else if (msg_type.value == error_packet_header)
    {
        // We don't know which DB is yet. The server has no knowledge of our capabilities
        // yet, so it will assume we don't support the 4.1 protocol and send an error
        // packet without SQL state
        return process_error_packet(ctx.to_span(), db_flavor::mysql, diag, false);
    }
    else if (msg_type.value != handshake_protocol_version_10)
    {
        return make_error_code(client_errc::protocol_value_error);
    }
    else
    {
        return deserialize_server_hello_impl(ctx.to_span(), output);
    }
}

// auth_switch
BOOST_ATTRIBUTE_NODISCARD
boost::mysql::error_code boost::mysql::detail::deserialize_auth_switch(
    span<const std::uint8_t> msg,
    auth_switch& output
)
{
    struct auth_switch_request_packet
    {
        string_null plugin_name;
        string_eof auth_plugin_data;
    } pack{};

    deserialization_context ctx(msg);

    auto err = ctx.deserialize(pack.plugin_name, pack.auth_plugin_data);
    if (err != deserialize_errc::ok)
        return to_error_code(err);

    // Discard an additional NULL at the end of auth data
    string_view auth_data = pack.auth_plugin_data.value;
    if (!auth_data.empty() && auth_data.back() == 0)
    {
        auth_data = auth_data.substr(0, auth_data.size() - 1);
    }

    output = {
        pack.plugin_name.value,
        to_span(auth_data),
    };

    return ctx.check_extra_bytes();
}

boost::mysql::detail::handshake_server_response boost::mysql::detail::deserialize_handshake_server_response(
    span<const std::uint8_t> buff,
    db_flavor flavor,
    diagnostics& diag
)
{
    constexpr std::uint8_t auth_switch_request_header = 0xfe;
    constexpr std::uint8_t auth_more_data_header = 0x01;

    deserialization_context ctx(buff);
    int1 msg_type{};
    auto err = to_error_code(msg_type.deserialize(ctx));
    if (err)
        return err;

    if (msg_type.value == ok_packet_header)
    {
        ok_view ok{};
        err = deserialize_ok_packet(ctx.to_span(), ok);
        if (err)
            return err;
        return ok;
    }
    else if (msg_type.value == error_packet_header)
    {
        return process_error_packet(ctx.to_span(), flavor, diag);
    }
    else if (msg_type.value == auth_switch_request_header)
    {
        // We have received an auth switch request. Deserialize it
        auth_switch auth_sw{};
        err = deserialize_auth_switch(ctx.to_span(), auth_sw);
        if (err)
            return err;
        return auth_sw;
    }
    else if (msg_type.value == auth_more_data_header)
    {
        // We have received an auth more data request. Deserialize it.
        // Note that string_eof never fails deserialization (by definition)
        string_eof auth_more_data;
        auto ec = auth_more_data.deserialize(ctx);
        BOOST_ASSERT(ec == deserialize_errc::ok);
        boost::ignore_unused(ec);
        return handshake_server_response(to_span(auth_more_data.value));
    }
    else
    {
        // Unknown message type
        return make_error_code(client_errc::protocol_value_error);
    }
}

#endif
