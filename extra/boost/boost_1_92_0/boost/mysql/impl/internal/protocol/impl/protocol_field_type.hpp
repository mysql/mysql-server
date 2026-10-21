//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_INTERNAL_PROTOCOL_IMPL_PROTOCOL_FIELD_TYPE_HPP
#define BOOST_MYSQL_IMPL_INTERNAL_PROTOCOL_IMPL_PROTOCOL_FIELD_TYPE_HPP

#include <boost/mysql/column_type.hpp>

#include <boost/mysql/detail/flags.hpp>

#include <cstdint>

namespace boost {
namespace mysql {
namespace detail {

enum class protocol_field_type : std::uint8_t
{
    decimal = 0x00,      // Apparently not sent
    tiny = 0x01,         // TINYINT
    short_ = 0x02,       // SMALLINT
    long_ = 0x03,        // INT
    float_ = 0x04,       // FLOAT
    double_ = 0x05,      // DOUBLE
    null = 0x06,         // Apparently not sent
    timestamp = 0x07,    // TIMESTAMP
    longlong = 0x08,     // BIGINT
    int24 = 0x09,        // MEDIUMINT
    date = 0x0a,         // DATE
    time = 0x0b,         // TIME
    datetime = 0x0c,     // DATETIME
    year = 0x0d,         // YEAR
    varchar = 0x0f,      // Apparently not sent
    bit = 0x10,          // BIT
    json = 0xf5,         // JSON
    newdecimal = 0xf6,   // DECIMAL
    enum_ = 0xf7,        // Apparently not sent
    set = 0xf8,          // Apparently not sent
    tiny_blob = 0xf9,    // Apparently not sent
    medium_blob = 0xfa,  // Apparently not sent
    long_blob = 0xfb,    // Apparently not sent
    blob = 0xfc,         // Used for all TEXT and BLOB types
    var_string = 0xfd,   // Used for VARCHAR and VARBINARY
    string = 0xfe,       // Used for CHAR and BINARY, ENUM (enum flag set), SET (set flag set)
    geometry = 0xff      // GEOMETRY
};

inline column_type compute_column_type(
    protocol_field_type protocol_type,
    std::uint16_t flags,
    std::uint16_t collation
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

BOOST_INLINE_CONSTEXPR std::uint16_t binary_collation = 63;  // used to distinguish blobs from strings

inline column_type compute_field_type_string(std::uint16_t flags, std::uint16_t collation)
{
    if (flags & column_flags::set)
        return column_type::set;
    else if (flags & column_flags::enum_)
        return column_type::enum_;
    else if (collation == binary_collation)
        return column_type::binary;
    else
        return column_type::char_;
}

inline column_type compute_field_type_var_string(std::uint16_t collation)
{
    return collation == binary_collation ? column_type::varbinary : column_type::varchar;
}

inline column_type compute_field_type_blob(std::uint16_t collation)
{
    return collation == binary_collation ? column_type::blob : column_type::text;
}

}  // namespace detail
}  // namespace mysql
}  // namespace boost

boost::mysql::column_type boost::mysql::detail::compute_column_type(
    protocol_field_type protocol_type,
    std::uint16_t flags,
    std::uint16_t collation
)
{
    // Some protocol_field_types seem to not be sent by the server. We've found instances
    // where some servers, with certain SQL statements, send some of the "apparently not sent"
    // types (e.g. MariaDB was sending medium_blob only if you SELECT TEXT variables - but not with TEXT
    // columns). So we've taken a defensive approach here
    switch (protocol_type)
    {
    case protocol_field_type::decimal:
    case protocol_field_type::newdecimal: return column_type::decimal;
    case protocol_field_type::geometry: return column_type::geometry;
    case protocol_field_type::tiny: return column_type::tinyint;
    case protocol_field_type::short_: return column_type::smallint;
    case protocol_field_type::int24: return column_type::mediumint;
    case protocol_field_type::long_: return column_type::int_;
    case protocol_field_type::longlong: return column_type::bigint;
    case protocol_field_type::float_: return column_type::float_;
    case protocol_field_type::double_: return column_type::double_;
    case protocol_field_type::bit: return column_type::bit;
    case protocol_field_type::date: return column_type::date;
    case protocol_field_type::datetime: return column_type::datetime;
    case protocol_field_type::timestamp: return column_type::timestamp;
    case protocol_field_type::time: return column_type::time;
    case protocol_field_type::year: return column_type::year;
    case protocol_field_type::json: return column_type::json;
    case protocol_field_type::enum_: return column_type::enum_;  // in theory not set
    case protocol_field_type::set: return column_type::set;      // in theory not set
    case protocol_field_type::string: return compute_field_type_string(flags, collation);
    case protocol_field_type::varchar:  // in theory not sent
    case protocol_field_type::var_string: return compute_field_type_var_string(collation);
    case protocol_field_type::tiny_blob:    // in theory not sent
    case protocol_field_type::medium_blob:  // in theory not sent
    case protocol_field_type::long_blob:    // in theory not sent
    case protocol_field_type::blob: return compute_field_type_blob(collation);
    default: return column_type::unknown;
    }
}

#endif
