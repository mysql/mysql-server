//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_FLAGS_HPP
#define BOOST_MYSQL_DETAIL_FLAGS_HPP

#include <boost/config.hpp>

#include <cstdint>

namespace boost {
namespace mysql {
namespace detail {

namespace column_flags {

BOOST_INLINE_CONSTEXPR std::uint16_t not_null = 1;             // Field can't be NULL.
BOOST_INLINE_CONSTEXPR std::uint16_t pri_key = 2;              // Field is part of a primary key.
BOOST_INLINE_CONSTEXPR std::uint16_t unique_key = 4;           // Field is part of a unique key.
BOOST_INLINE_CONSTEXPR std::uint16_t multiple_key = 8;         // Field is part of a key.
BOOST_INLINE_CONSTEXPR std::uint16_t blob = 16;                // Field is a blob.
BOOST_INLINE_CONSTEXPR std::uint16_t unsigned_ = 32;           // Field is unsigned.
BOOST_INLINE_CONSTEXPR std::uint16_t zerofill = 64;            // Field is zerofill.
BOOST_INLINE_CONSTEXPR std::uint16_t binary = 128;             // Field is binary.
BOOST_INLINE_CONSTEXPR std::uint16_t enum_ = 256;              // field is an enum
BOOST_INLINE_CONSTEXPR std::uint16_t auto_increment = 512;     // field is a autoincrement field
BOOST_INLINE_CONSTEXPR std::uint16_t timestamp = 1024;         // Field is a timestamp.
BOOST_INLINE_CONSTEXPR std::uint16_t set = 2048;               // field is a set
BOOST_INLINE_CONSTEXPR std::uint16_t no_default_value = 4096;  // Field doesn't have default value.
BOOST_INLINE_CONSTEXPR std::uint16_t on_update_now = 8192;     // Field is set to NOW on UPDATE.
BOOST_INLINE_CONSTEXPR std::uint16_t part_key = 16384;         // Intern; Part of some key.
BOOST_INLINE_CONSTEXPR std::uint16_t num = 32768;              // Field is num (for clients)

}  // namespace column_flags

namespace status_flags {

BOOST_INLINE_CONSTEXPR std::uint32_t more_results = 8;
BOOST_INLINE_CONSTEXPR std::uint32_t no_backslash_escapes = 512;
BOOST_INLINE_CONSTEXPR std::uint32_t out_params = 4096;

}  // namespace status_flags

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
