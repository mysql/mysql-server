//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_INTERNAL_PROTOCOL_IMPL_PROTOCOL_TYPES_HPP
#define BOOST_MYSQL_IMPL_INTERNAL_PROTOCOL_IMPL_PROTOCOL_TYPES_HPP

#include <boost/mysql/string_view.hpp>

#include <boost/mysql/impl/internal/protocol/impl/deserialization_context.hpp>
#include <boost/mysql/impl/internal/protocol/impl/serialization_context.hpp>
#include <boost/mysql/impl/internal/protocol/impl/span_string.hpp>

#include <boost/endian/conversion.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace boost {
namespace mysql {
namespace detail {

// Integers
template <class IntType>
struct int_holder
{
    IntType value;

    // This is a fixed-size type
    static constexpr std::size_t size = sizeof(IntType);

    void serialize_fixed(std::uint8_t* to) const
    {
        endian::endian_store<IntType, sizeof(IntType), endian::order::little>(to, value);
    }

    void serialize(serialization_context& ctx) const { ctx.serialize_fixed(*this); }

    deserialize_errc deserialize(deserialization_context& ctx)
    {
        constexpr std::size_t sz = sizeof(IntType);
        if (!ctx.enough_size(sz))
        {
            return deserialize_errc::incomplete_message;
        }
        value = endian::endian_load<IntType, sz, boost::endian::order::little>(ctx.first());
        ctx.advance(sz);
        return deserialize_errc::ok;
    }
};

using int1 = int_holder<std::uint8_t>;
using int2 = int_holder<std::uint16_t>;
using int4 = int_holder<std::uint32_t>;
using int8 = int_holder<std::uint64_t>;
using sint8 = int_holder<std::int64_t>;

struct int3
{
    std::uint32_t value;

    // This is a fixed-size type
    static constexpr std::size_t size = 3u;

    void serialize_fixed(std::uint8_t* to) const { endian::store_little_u24(to, value); }

    void serialize(serialization_context& ctx) const { ctx.serialize_fixed(*this); }

    deserialize_errc deserialize(deserialization_context& ctx)
    {
        if (!ctx.enough_size(3))
            return deserialize_errc::incomplete_message;
        value = endian::load_little_u24(ctx.first());
        ctx.advance(3);
        return deserialize_errc::ok;
    }
};

struct int_lenenc
{
    std::uint64_t value;

    void serialize(serialization_context& ctx) const
    {
        if (value < 251)
        {
            ctx.add(static_cast<std::uint8_t>(value));
        }
        else if (value < 0x10000)
        {
            ctx.serialize_fixed(
                int1{static_cast<std::uint8_t>(0xfc)},
                int2{static_cast<std::uint16_t>(value)}
            );
        }
        else if (value < 0x1000000)
        {
            ctx.serialize_fixed(
                int1{static_cast<std::uint8_t>(0xfd)},
                int3{static_cast<std::uint32_t>(value)}
            );
        }
        else
        {
            ctx.serialize_fixed(int1{static_cast<std::uint8_t>(0xfe)}, int8{value});
        }
    }

    deserialize_errc deserialize(deserialization_context& ctx)
    {
        int1 first_byte{};
        auto err = first_byte.deserialize(ctx);
        if (err != deserialize_errc::ok)
        {
            return err;
        }

        if (first_byte.value == 0xfc)
        {
            int2 int2_value{};
            err = int2_value.deserialize(ctx);
            value = int2_value.value;
        }
        else if (first_byte.value == 0xfd)
        {
            int3 int3_value{};
            err = int3_value.deserialize(ctx);
            value = int3_value.value;
        }
        else if (first_byte.value == 0xfe)
        {
            int8 int8_value{};
            err = int8_value.deserialize(ctx);
            value = int8_value.value;
        }
        else
        {
            err = deserialize_errc::ok;
            value = first_byte.value;
        }
        return err;
    }
};

struct string_null
{
    string_view value;

    void serialize(serialization_context& ctx) const
    {
        ctx.add(to_span(value));
        ctx.add(static_cast<std::uint8_t>(0));  // null terminator
    }

    deserialize_errc deserialize(deserialization_context& ctx)
    {
        auto string_end = std::find(ctx.first(), ctx.last(), 0);
        if (string_end == ctx.last())
        {
            return deserialize_errc::incomplete_message;
        }
        std::size_t length = string_end - ctx.first();
        value = ctx.get_string(length);
        ctx.advance(length + 1);  // skip the null terminator
        return deserialize_errc::ok;
    }
};

struct string_eof
{
    string_view value;

    deserialize_errc deserialize(deserialization_context& ctx)
    {
        std::size_t size = ctx.size();
        value = ctx.get_string(size);
        ctx.advance(size);
        return deserialize_errc::ok;
    }

    void serialize(serialization_context& ctx) const { ctx.add(to_span(value)); }
};

struct string_lenenc
{
    string_view value;

    deserialize_errc deserialize(deserialization_context& ctx)
    {
        int_lenenc length;
        auto err = length.deserialize(ctx);
        if (err != deserialize_errc::ok)
        {
            return err;
        }
        if (length.value > (std::numeric_limits<std::size_t>::max)())
        {
            return deserialize_errc::protocol_value_error;
        }
        auto len = static_cast<std::size_t>(length.value);
        if (!ctx.enough_size(len))
        {
            return deserialize_errc::incomplete_message;
        }

        value = ctx.get_string(len);
        ctx.advance(len);
        return deserialize_errc::ok;
    }

    void serialize(serialization_context& ctx) const
    {
        ctx.serialize(int_lenenc{value.size()});
        ctx.add(to_span(value));
    }
};

template <std::size_t N>
struct string_fixed
{
    std::array<char, N> value;

    // This is a fixed size type
    static constexpr std::size_t size = N;

    void serialize_fixed(std::uint8_t* to) const { std::memcpy(to, value.data(), N); }

    void serialize(serialization_context& ctx) const { ctx.serialize_fixed(*this); }

    deserialize_errc deserialize(deserialization_context& ctx)
    {
        if (!ctx.enough_size(N))
            return deserialize_errc::incomplete_message;
        std::memcpy(value.data(), ctx.first(), N);
        ctx.advance(N);
        return deserialize_errc::ok;
    }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
