//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_INTERNAL_PROTOCOL_IMPL_BINARY_PROTOCOL_HPP
#define BOOST_MYSQL_IMPL_INTERNAL_PROTOCOL_IMPL_BINARY_PROTOCOL_HPP

#include <boost/mysql/days.hpp>
#include <boost/mysql/field_kind.hpp>
#include <boost/mysql/field_view.hpp>
#include <boost/mysql/metadata.hpp>

#include <boost/mysql/detail/datetime.hpp>

#include <boost/mysql/impl/internal/protocol/impl/bit_deserialization.hpp>
#include <boost/mysql/impl/internal/protocol/impl/deserialization_context.hpp>
#include <boost/mysql/impl/internal/protocol/impl/protocol_types.hpp>
#include <boost/mysql/impl/internal/protocol/impl/serialization_context.hpp>

#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>

namespace boost {
namespace mysql {
namespace detail {

inline deserialize_errc deserialize_binary_field(
    deserialization_context& ctx,
    const metadata& meta,
    field_view& output
);

inline void serialize_binary_field(serialization_context& ctx, field_view input);

}  // namespace detail
}  // namespace mysql
}  // namespace boost

//
// Implementation
//

namespace boost {
namespace mysql {
namespace detail {

// constants
namespace binc {

BOOST_INLINE_CONSTEXPR std::size_t year_sz = 2;
BOOST_INLINE_CONSTEXPR std::size_t month_sz = 1;
BOOST_INLINE_CONSTEXPR std::size_t date_day_sz = 1;
BOOST_INLINE_CONSTEXPR std::size_t time_days_sz = 4;
BOOST_INLINE_CONSTEXPR std::size_t hours_sz = 1;
BOOST_INLINE_CONSTEXPR std::size_t mins_sz = 1;
BOOST_INLINE_CONSTEXPR std::size_t secs_sz = 1;
BOOST_INLINE_CONSTEXPR std::size_t micros_sz = 4;
BOOST_INLINE_CONSTEXPR std::size_t time_sign_sz = 1;

BOOST_INLINE_CONSTEXPR std::size_t date_sz = year_sz + month_sz + date_day_sz;  // does not include length

BOOST_INLINE_CONSTEXPR std::size_t datetime_d_sz = date_sz;
BOOST_INLINE_CONSTEXPR std::size_t datetime_dhms_sz = datetime_d_sz + hours_sz + mins_sz + secs_sz;
BOOST_INLINE_CONSTEXPR std::size_t datetime_dhmsu_sz = datetime_dhms_sz + micros_sz;

BOOST_INLINE_CONSTEXPR std::size_t time_dhms_sz = time_sign_sz + time_days_sz + hours_sz + mins_sz + secs_sz;
BOOST_INLINE_CONSTEXPR std::size_t time_dhmsu_sz = time_dhms_sz + micros_sz;

}  // namespace binc

//
// Deserialization
//

// strings
inline deserialize_errc deserialize_binary_field_string(
    deserialization_context& ctx,
    field_view& output,
    bool is_blob
)
{
    string_lenenc deser;
    auto err = deser.deserialize(ctx);
    if (err != deserialize_errc::ok)
        return err;
    if (is_blob)
    {
        output = field_view(to_span(deser.value));
    }
    else
    {
        output = field_view(deser.value);
    }
    return deserialize_errc::ok;
}

// ints
template <class TargetType, class DeserializableType>
inline deserialize_errc deserialize_binary_field_int_impl(deserialization_context& ctx, field_view& output)
{
    int_holder<DeserializableType> deser;
    auto err = deser.deserialize(ctx);
    if (err != deserialize_errc::ok)
        return err;
    output = field_view(static_cast<TargetType>(deser.value));
    return deserialize_errc::ok;
}

template <class DeserializableTypeUnsigned, class DeserializableTypeSigned>
inline deserialize_errc deserialize_binary_field_int(
    const metadata& meta,
    deserialization_context& ctx,
    field_view& output
)
{
    return meta.is_unsigned()
               ? deserialize_binary_field_int_impl<std::uint64_t, DeserializableTypeUnsigned>(ctx, output)
               : deserialize_binary_field_int_impl<std::int64_t, DeserializableTypeSigned>(ctx, output);
}

// Bits. These come as a binary value between 1 and 8 bytes,
// packed in a string
inline deserialize_errc deserialize_binary_field_bit(deserialization_context& ctx, field_view& output)
{
    string_lenenc buffer;
    auto err = buffer.deserialize(ctx);
    if (err != deserialize_errc::ok)
        return err;
    return deserialize_bit(buffer.value, output);
}

// Floats
template <class T>
inline deserialize_errc deserialize_binary_field_float(deserialization_context& ctx, field_view& output)
{
    // Size check
    if (!ctx.enough_size(sizeof(T)))
        return deserialize_errc::incomplete_message;

    // Endianness conversion
    T v = endian::endian_load<T, sizeof(T), endian::order::little>(ctx.first());

    // Nans and infs not allowed in SQL
    if (std::isnan(v) || std::isinf(v))
        return deserialize_errc::protocol_value_error;

    // Done
    ctx.advance(sizeof(T));
    output = field_view(v);
    return deserialize_errc::ok;
}

// Time types
inline deserialize_errc deserialize_binary_ymd(deserialization_context& ctx, date& output)
{
    using namespace boost::mysql::detail;

    int2 year{};
    int1 month{};
    int1 day{};

    // Deserialize
    auto err = ctx.deserialize(year, month, day);
    if (err != deserialize_errc::ok)
        return err;

    // Range check
    if (year.value > max_year || month.value > max_month || day.value > max_day)
    {
        return deserialize_errc::protocol_value_error;
    }

    output = date(year.value, month.value, day.value);

    return deserialize_errc::ok;
}

inline deserialize_errc deserialize_binary_field_date(deserialization_context& ctx, field_view& output)
{
    using namespace boost::mysql::detail::binc;

    // Deserialize length
    int1 length;
    auto err = length.deserialize(ctx);
    if (err != deserialize_errc::ok)
        return err;

    // Check for zero dates
    if (length.value < date_sz)
    {
        output = field_view(date());
        return deserialize_errc::ok;
    }

    // Deserialize rest of fields
    date d;
    err = deserialize_binary_ymd(ctx, d);
    if (err != deserialize_errc::ok)
        return err;
    output = field_view(d);
    return deserialize_errc::ok;
}

inline deserialize_errc deserialize_binary_field_datetime(deserialization_context& ctx, field_view& output)
{
    using namespace binc;

    // Deserialize length
    int1 length{};
    auto err = length.deserialize(ctx);
    if (err != deserialize_errc::ok)
        return err;

    // If the DATETIME does not contain some of the values below,
    // they are supposed to be zero
    date d{};
    int1 hours{};
    int1 minutes{};
    int1 seconds{};
    int4 micros{};

    // Date part
    if (length.value >= datetime_d_sz)
    {
        err = deserialize_binary_ymd(ctx, d);
        if (err != deserialize_errc::ok)
            return err;
    }

    // Hours, minutes, seconds
    if (length.value >= datetime_dhms_sz)
    {
        err = ctx.deserialize(hours, minutes, seconds);
        if (err != deserialize_errc::ok)
            return err;
    }

    // Microseconds
    if (length.value >= datetime_dhmsu_sz)
    {
        err = micros.deserialize(ctx);
        if (err != deserialize_errc::ok)
            return err;
    }

    // Validity check. deserialize_binary_ymd already does it for date
    if (hours.value > max_hour || minutes.value > max_min || seconds.value > max_sec ||
        micros.value > max_micro)
    {
        return deserialize_errc::protocol_value_error;
    }

    // Compose the final datetime
    datetime dt(d.year(), d.month(), d.day(), hours.value, minutes.value, seconds.value, micros.value);
    output = field_view(dt);
    return deserialize_errc::ok;
}

inline deserialize_errc deserialize_binary_field_time(deserialization_context& ctx, field_view& output)
{
    using namespace boost::mysql::detail;
    using namespace boost::mysql::detail::binc;

    // Deserialize length
    int1 length{};
    auto err = length.deserialize(ctx);
    if (err != deserialize_errc::ok)
        return err;

    // If the TIME contains no value for these fields, they are zero
    int1 is_negative{};
    int4 num_days{};
    int1 hours{};
    int1 minutes{};
    int1 seconds{};
    int4 microseconds{};

    // Sign, days, hours, minutes, seconds
    if (length.value >= time_dhms_sz)
    {
        err = ctx.deserialize(is_negative, num_days, hours, minutes, seconds);
        if (err != deserialize_errc::ok)
            return err;
    }

    // Microseconds
    if (length.value >= time_dhmsu_sz)
    {
        err = microseconds.deserialize(ctx);
        if (err != deserialize_errc::ok)
            return err;
    }

    // Range check
    constexpr std::size_t time_max_days = 34;  // equivalent to the 839 hours, in the broken format
    if (num_days.value > time_max_days || hours.value > max_hour || minutes.value > max_min ||
        seconds.value > max_sec || microseconds.value > max_micro)
    {
        return deserialize_errc::protocol_value_error;
    }

    // Compose the final time
    output = field_view(boost::mysql::time(
        (is_negative.value ? -1 : 1) *
        (days(num_days.value) + std::chrono::hours(hours.value) + std::chrono::minutes(minutes.value) +
         std::chrono::seconds(seconds.value) + std::chrono::microseconds(microseconds.value))
    ));
    return deserialize_errc::ok;
}

//
// Serialization
//
template <class T>
inline void serialize_binary_float(serialization_context& ctx, T input)
{
    std::array<std::uint8_t, sizeof(T)> buffer{};
    boost::endian::endian_store<T, sizeof(T), boost::endian::order::little>(buffer.data(), input);
    ctx.add(buffer);
}

inline void serialize_binary_date(serialization_context& ctx, const date& input)
{
    ctx.serialize_fixed(
        int1{static_cast<std::uint8_t>(binc::date_sz)},
        int2{input.year()},
        int1{input.month()},
        int1{input.day()}
    );
}

inline void serialize_binary_datetime(serialization_context& ctx, const datetime& input)
{
    ctx.serialize_fixed(
        int1{static_cast<std::uint8_t>(binc::datetime_dhmsu_sz)},
        int2{input.year()},
        int1{input.month()},
        int1{input.day()},
        int1{input.hour()},
        int1{input.minute()},
        int1{input.second()},
        int4{input.microsecond()}
    );
}

inline void serialize_binary_time(serialization_context& ctx, const boost::mysql::time& input)
{
    using namespace binc;
    using boost::mysql::days;
    using std::chrono::duration_cast;
    using std::chrono::hours;
    using std::chrono::microseconds;
    using std::chrono::minutes;
    using std::chrono::seconds;

    // Break time
    auto num_micros = duration_cast<microseconds>(input % seconds(1));
    auto num_secs = duration_cast<seconds>(input % minutes(1) - num_micros);
    auto num_mins = duration_cast<minutes>(input % hours(1) - num_secs);
    auto num_hours = duration_cast<hours>(input % days(1) - num_mins);
    auto num_days = duration_cast<days>(input - num_hours);
    auto is_negative = (input.count() < 0) ? 1 : 0;

    // Serialize
    ctx.serialize_fixed(
        int1{static_cast<std::uint8_t>(time_dhmsu_sz)},
        int1{static_cast<std::uint8_t>(is_negative)},
        int4{static_cast<std::uint32_t>(std::abs(num_days.count()))},
        int1{static_cast<std::uint8_t>(std::abs(num_hours.count()))},
        int1{static_cast<std::uint8_t>(std::abs(num_mins.count()))},
        int1{static_cast<std::uint8_t>(std::abs(num_secs.count()))},
        int4{static_cast<std::uint32_t>(std::abs(num_micros.count()))}
    );
}

}  // namespace detail
}  // namespace mysql
}  // namespace boost

boost::mysql::detail::deserialize_errc boost::mysql::detail::deserialize_binary_field(
    deserialization_context& ctx,
    const metadata& meta,
    field_view& output
)
{
    switch (meta.type())
    {
    case column_type::tinyint:
        return deserialize_binary_field_int<std::uint8_t, std::int8_t>(meta, ctx, output);
    case column_type::smallint:
    case column_type::year:
        return deserialize_binary_field_int<std::uint16_t, std::int16_t>(meta, ctx, output);
    case column_type::mediumint:
    case column_type::int_:
        return deserialize_binary_field_int<std::uint32_t, std::int32_t>(meta, ctx, output);
    case column_type::bigint:
        return deserialize_binary_field_int<std::uint64_t, std::int64_t>(meta, ctx, output);
    case column_type::bit: return deserialize_binary_field_bit(ctx, output);
    case column_type::float_: return deserialize_binary_field_float<float>(ctx, output);
    case column_type::double_: return deserialize_binary_field_float<double>(ctx, output);
    case column_type::timestamp:
    case column_type::datetime: return deserialize_binary_field_datetime(ctx, output);
    case column_type::date: return deserialize_binary_field_date(ctx, output);
    case column_type::time: return deserialize_binary_field_time(ctx, output);
    // True string types
    case column_type::char_:
    case column_type::varchar:
    case column_type::text:
    case column_type::enum_:
    case column_type::set:
    case column_type::decimal:
    case column_type::json: return deserialize_binary_field_string(ctx, output, false);
    // Blobs and anything else
    case column_type::binary:
    case column_type::varbinary:
    case column_type::blob:
    case column_type::geometry:
    default: return deserialize_binary_field_string(ctx, output, true);
    }
}

void boost::mysql::detail::serialize_binary_field(serialization_context& ctx, field_view input)
{
    switch (input.kind())
    {
    case field_kind::null: break;
    case field_kind::int64: sint8{input.get_int64()}.serialize(ctx); break;
    case field_kind::uint64: int8{input.get_uint64()}.serialize(ctx); break;
    case field_kind::string: string_lenenc{input.get_string()}.serialize(ctx); break;
    case field_kind::blob: string_lenenc{to_string(input.get_blob())}.serialize(ctx); break;
    case field_kind::float_: serialize_binary_float(ctx, input.get_float()); break;
    case field_kind::double_: serialize_binary_float(ctx, input.get_double()); break;
    case field_kind::date: serialize_binary_date(ctx, input.get_date()); break;
    case field_kind::datetime: serialize_binary_datetime(ctx, input.get_datetime()); break;
    case field_kind::time: serialize_binary_time(ctx, input.get_time()); break;
    default: BOOST_ASSERT(false); break;  // LCOV_EXCL_LINE
    }
}

#endif /* INCLUDE_BOOST_MYSQL_DETAIL_PROTOCOL_BINARY_DESERIALIZATION_HPP_ */
