//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_INTERNAL_PROTOCOL_FRAME_HEADER_HPP
#define BOOST_MYSQL_IMPL_INTERNAL_PROTOCOL_FRAME_HEADER_HPP

#include <boost/assert.hpp>
#include <boost/config.hpp>
#include <boost/core/span.hpp>
#include <boost/endian/conversion.hpp>

#include <cstddef>

namespace boost {
namespace mysql {
namespace detail {

BOOST_INLINE_CONSTEXPR std::size_t max_packet_size = 0xffffff;
BOOST_INLINE_CONSTEXPR std::size_t frame_header_size = 4;

struct frame_header
{
    std::uint32_t size;
    std::uint8_t sequence_number;
};

inline void serialize_frame_header(span<std::uint8_t, frame_header_size> to, frame_header header)
{
    BOOST_ASSERT(header.size <= 0xffffff);
    endian::store_little_u24(to.data(), header.size);
    to[3] = header.sequence_number;
}

inline frame_header deserialize_frame_header(span<const std::uint8_t, frame_header_size> buffer)
{
    return {endian::load_little_u24(buffer.data()), buffer[3]};
}

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
