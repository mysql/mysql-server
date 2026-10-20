//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_INTERNAL_PROTOCOL_IMPL_NULL_BITMAP_HPP
#define BOOST_MYSQL_IMPL_INTERNAL_PROTOCOL_IMPL_NULL_BITMAP_HPP

#include <boost/mysql/field_view.hpp>

#include <boost/assert.hpp>
#include <boost/config.hpp>
#include <boost/core/span.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace boost {
namespace mysql {
namespace detail {

// When parsing binary rows, we need to add this offset to
// field positions to get the actual field index to use -
// the first two positions are reserved
BOOST_INLINE_CONSTEXPR std::size_t binary_row_null_bitmap_offset = 2;

// Helper to parse the null bitmap contained in binary rows
class null_bitmap_parser
{
    std::size_t num_fields_;

public:
    constexpr null_bitmap_parser(std::size_t num_fields) noexcept : num_fields_{num_fields} {};
    constexpr std::size_t byte_count() const { return (num_fields_ + 7 + binary_row_null_bitmap_offset) / 8; }
    bool is_null(const std::uint8_t* first, std::size_t field_pos) const
    {
        BOOST_ASSERT(field_pos < num_fields_);

        std::size_t byte_pos = (field_pos + binary_row_null_bitmap_offset) / 8;
        std::size_t bit_pos = (field_pos + binary_row_null_bitmap_offset) % 8;
        return first[byte_pos] & (1 << bit_pos);
    }
};

class null_bitmap_generator
{
    span<const field_view> fields_;
    std::size_t current_{0};

public:
    null_bitmap_generator(span<const field_view> fields) noexcept : fields_(fields) {}
    bool done() const { return current_ == fields_.size(); }
    std::uint8_t next()
    {
        BOOST_ASSERT(current_ < fields_.size());

        std::uint8_t res = 0;

        // Generate
        const std::size_t max_i = (std::min)(fields_.size(), current_ + 8u);
        for (std::size_t i = current_; i < max_i; ++i)
        {
            if (fields_[i].is_null())
            {
                const auto bit_pos = i % 8;
                res |= (1 << bit_pos);
            }
        }

        // Update state
        current_ = max_i;

        // Return
        return res;
    }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif /* INCLUDE_NULL_BITMAP_HPP_ */
