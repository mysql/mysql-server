//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_FORMAT_SQL_IPP
#define BOOST_MYSQL_IMPL_FORMAT_SQL_IPP

#include <boost/mysql/blob_view.hpp>
#include <boost/mysql/character_set.hpp>
#include <boost/mysql/client_errc.hpp>
#include <boost/mysql/constant_string_view.hpp>
#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/field_kind.hpp>
#include <boost/mysql/field_view.hpp>
#include <boost/mysql/format_sql.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/mysql/detail/access.hpp>
#include <boost/mysql/detail/escape_string.hpp>
#include <boost/mysql/detail/format_sql.hpp>
#include <boost/mysql/detail/output_string.hpp>

#include <boost/mysql/impl/internal/byte_to_hex.hpp>
#include <boost/mysql/impl/internal/call_next_char.hpp>
#include <boost/mysql/impl/internal/dt_to_string.hpp>

#include <boost/charconv/from_chars.hpp>
#include <boost/charconv/to_chars.hpp>
#include <boost/core/detail/string_view.hpp>
#include <boost/system/result.hpp>
#include <boost/system/system_error.hpp>
#include <boost/throw_exception.hpp>

#include <cmath>
#include <cstddef>
#include <limits>
#include <string>

namespace boost {
namespace mysql {
namespace detail {

// Helpers to format fundamental types
inline void append_quoted_identifier(string_view name, format_context_base& ctx)
{
    ctx.append_raw("`");
    auto& impl = access::get_impl(ctx);
    auto ec = detail::escape_string(name, impl.opts, '`', impl.output);
    if (ec)
        ctx.add_error(ec);
    ctx.append_raw("`");
}

template <class T>
void append_int(T integer, format_context_base& ctx)
{
    // Make sure our buffer is big enough. 2: sign + digits10 is only 1 below max
    constexpr std::size_t buffsize = 32;
    static_assert(2 + std::numeric_limits<double>::digits10 < buffsize, "");

    char buff[buffsize];

    auto res = charconv::to_chars(buff, buff + buffsize, integer);

    // Can only fail because of buffer being too small
    BOOST_ASSERT(res.ec == std::errc());

    // Copy
    access::get_impl(ctx).output.append(string_view(buff, res.ptr - buff));
}

inline void append_double(double number, format_context_base& ctx)
{
    // Make sure our buffer is big enough. 4: sign, radix point, e+
    // 3: max exponent digits
    constexpr std::size_t buffsize = 32;
    static_assert(4 + std::numeric_limits<double>::max_digits10 + 3 < buffsize, "");

    // inf and nan are not supported by MySQL
    if (std::isinf(number) || std::isnan(number))
    {
        ctx.add_error(client_errc::unformattable_value);
        return;
    }

    char buff[buffsize];

    // We format as scientific to make MySQL understand the number as a double.
    // Otherwise, it takes it as a DECIMAL.
    auto res = charconv::to_chars(buff, buff + buffsize, number, charconv::chars_format::scientific);

    // Can only fail because of buffer being too small
    BOOST_ASSERT(res.ec == std::errc());

    // Copy
    access::get_impl(ctx).output.append(string_view(buff, res.ptr - buff));
}

inline void append_quoted_string(string_view str, format_context_base& ctx)
{
    auto& impl = access::get_impl(ctx);
    impl.output.append("'");
    auto ec = detail::escape_string(str, impl.opts, '\'', impl.output);
    if (ec)
        ctx.add_error(ec);
    impl.output.append("'");
}

inline void append_string(string_view str, string_view format_spec, format_context_base& ctx)
{
    // Parse format spec
    if (format_spec.size() > 1u)
    {
        ctx.add_error(client_errc::format_string_invalid_specifier);
        return;
    }

    // No specifier: quoted string
    if (format_spec.empty())
        return append_quoted_string(str, ctx);

    // We got a specifier
    switch (format_spec[0])
    {
    case 'i':
        // format as identifier
        return append_quoted_identifier(str, ctx);
    case 'r':
        // append raw SQL
        ctx.append_raw(runtime(str));
        break;
    default: ctx.add_error(client_errc::format_string_invalid_specifier);
    }
}

inline void append_blob(blob_view b, format_context_base& ctx)
{
    // Blobs have a binary character set, which may include characters
    // that are not valid in the current character set. However, escaping
    // is always performed using the character_set_connection.
    // mysql_real_escape_string escapes multibyte characters with a backslash,
    // but this behavior is not documented, so we don't want to rely on it.
    // The most reliable way to encode blobs is using hex strings.

    // Output string
    auto output = access::get_impl(ctx).output;

    // We output characters to a temporary buffer, batching append calls
    constexpr std::size_t buffer_size = 64;
    char buffer[buffer_size]{};
    char* it = buffer;
    char* const end = buffer + buffer_size;

    // Binary string introducer
    output.append("x'");

    // Serialize contents
    for (unsigned char byte : b)
    {
        // Serialize the byte
        it = byte_to_hex(byte, it);

        // If we filled the buffer, dump it
        if (it == end)
        {
            output.append({buffer, buffer_size});
            it = buffer;
        }
    }

    // Dump anything that didn't fill the buffer
    output.append({buffer, static_cast<std::size_t>(it - buffer)});

    // Closing quote
    ctx.append_raw("'");
}

inline void append_quoted_date(date d, format_context_base& ctx)
{
    char buffer[34];
    buffer[0] = '\'';
    std::size_t sz = detail::date_to_string(d.year(), d.month(), d.day(), span<char, 32>(buffer + 1, 32));
    buffer[sz + 1] = '\'';
    access::get_impl(ctx).output.append(string_view(buffer, sz + 2));
}

inline void append_quoted_datetime(datetime d, format_context_base& ctx)
{
    char buffer[66];
    buffer[0] = '\'';
    std::size_t sz = detail::datetime_to_string(
        d.year(),
        d.month(),
        d.day(),
        d.hour(),
        d.minute(),
        d.second(),
        d.microsecond(),
        span<char, 64>(buffer + 1, 64)
    );
    buffer[sz + 1] = '\'';
    access::get_impl(ctx).output.append(string_view(buffer, sz + 2));
}

inline void append_quoted_time(time t, format_context_base& ctx)
{
    char buffer[66];
    buffer[0] = '\'';
    std::size_t sz = time_to_string(t, span<char, 64>(buffer + 1, 64));
    buffer[sz + 1] = '\'';
    access::get_impl(ctx).output.append(string_view(buffer, sz + 2));
}

inline void append_field_view(
    field_view fv,
    string_view format_spec,
    bool allow_specs,
    format_context_base& ctx
)
{
    auto kind = fv.kind();

    // String types may allow specs
    if (allow_specs && kind == field_kind::string)
    {
        append_string(fv.get_string(), format_spec, ctx);
        return;
    }

    // Reject specifiers if !allow_specs or for other types
    if (!format_spec.empty())
    {
        ctx.add_error(client_errc::format_string_invalid_specifier);
        return;
    }

    // Perform the formatting operation
    switch (fv.kind())
    {
    case field_kind::null: ctx.append_raw("NULL"); return;
    case field_kind::int64: return append_int(fv.get_int64(), ctx);
    case field_kind::uint64: return append_int(fv.get_uint64(), ctx);
    case field_kind::float_:
        // float is formatted as double because it's parsed as such
        return append_double(fv.get_float(), ctx);
    case field_kind::double_: return append_double(fv.get_double(), ctx);
    case field_kind::string: return append_quoted_string(fv.get_string(), ctx);
    case field_kind::blob: return append_blob(fv.get_blob(), ctx);
    case field_kind::date: return append_quoted_date(fv.get_date(), ctx);
    case field_kind::datetime: return append_quoted_datetime(fv.get_datetime(), ctx);
    case field_kind::time: return append_quoted_time(fv.get_time(), ctx);
    default: BOOST_ASSERT(false); return;  // LCOV_EXCL_LINE
    }
}

// Helpers for parsing format strings
inline bool is_number(char c) { return c >= '0' && c <= '9'; }

inline bool is_name_start(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; }

inline bool is_format_spec_char(char c)
{
    return c != '{' && c != '}' && static_cast<unsigned char>(c) >= 0x20 &&
           static_cast<unsigned char>(c) <= 0x7e;
}

class format_state
{
    format_context_base& ctx_;
    span<const format_arg> args_;

    // Borrowed from fmt
    // 0: we haven't used any args yet
    // -1: we're doing explicit indexing
    // >0: we're doing auto indexing
    int next_arg_id_{0};

    BOOST_ATTRIBUTE_NODISCARD
    bool advance(const char*& it, const char* end)
    {
        std::size_t size = detail::call_next_char(ctx_.impl_.opts.charset, it, end);
        if (size == 0)
        {
            ctx_.add_error(client_errc::format_string_invalid_encoding);
            return false;
        }
        it += size;
        return true;
    }

    bool uses_auto_ids() const noexcept { return next_arg_id_ > 0; }
    bool uses_explicit_ids() const noexcept { return next_arg_id_ == -1; }

    void do_field(format_arg arg, string_view format_spec)
    {
        ctx_.format_arg(access::get_impl(arg).value, format_spec);
    }

    BOOST_ATTRIBUTE_NODISCARD
    bool do_indexed_field(int arg_id, string_view format_spec)
    {
        BOOST_ASSERT(arg_id >= 0);
        if (static_cast<std::size_t>(arg_id) >= args_.size())
        {
            ctx_.add_error(client_errc::format_arg_not_found);
            return false;
        }
        do_field(args_[arg_id], format_spec);
        return true;
    }

    struct arg_id_t
    {
        enum class type_t
        {
            none,
            integral,
            identifier
        };
        union data_t
        {
            unsigned short integral;
            string_view identifier;

            data_t() noexcept : integral{} {}
        };

        type_t type;
        data_t data;

        arg_id_t() noexcept : type(type_t::none), data() {}
        arg_id_t(unsigned short v) noexcept : type(type_t::integral) { data.integral = v; }
        arg_id_t(string_view v) noexcept : type(type_t::identifier) { data.identifier = v; }
    };

    BOOST_ATTRIBUTE_NODISCARD
    static arg_id_t parse_arg_id(const char*& it, const char* format_end)
    {
        if (is_number(*it))
        {
            unsigned short field_index = 0;
            auto res = charconv::from_chars(it, format_end, field_index);
            if (res.ec != std::errc{})
                return arg_id_t();
            it = res.ptr;
            return field_index;
        }
        else if (is_name_start(*it))
        {
            const char* name_begin = it;
            while (it != format_end && (is_name_start(*it) || is_number(*it)))
                ++it;
            string_view field_name(name_begin, it);
            return field_name;
        }
        else
        {
            return arg_id_t();
        }
    }

    BOOST_ATTRIBUTE_NODISCARD
    static string_view parse_format_spec(const char*& it, const char* format_end)
    {
        if (it != format_end && *it == ':')
        {
            ++it;
            const char* first = it;
            while (it != format_end && is_format_spec_char(*it))
                ++it;
            return {first, it};
        }
        else
        {
            return string_view();
        }
    }

    BOOST_ATTRIBUTE_NODISCARD
    bool parse_field(const char*& it, const char* format_end)
    {
        // Taken from fmtlib and adapted to our requirements
        // it points to the character next to the opening '{'
        // replacement_field ::=  "{" [arg_id] [":" (format_spec)] "}"
        // arg_id            ::=  integer | identifier
        // integer           ::=  <decimal, unsigned short, parsed by from_chars>
        // identifier        ::=  id_start id_continue*
        // id_start          ::=  "a"..."z" | "A"..."Z" | "_"
        // id_continue       ::=  id_start | digit
        // digit             ::=  "0"..."9"
        // format_spec       ::=  <any character >= 0x20 && <= 0x7e && != "{", "}">

        // Parse the ID and spec components
        auto arg_id = parse_arg_id(it, format_end);
        auto spec = parse_format_spec(it, format_end);

        // If we're not at the end on the string, it's a syntax error
        if (it == format_end || *it != '}')
        {
            ctx_.add_error(client_errc::format_string_invalid_syntax);
            return false;
        }
        ++it;

        // Process what was parsed
        switch (arg_id.type)
        {
        case arg_id_t::type_t::none: return append_auto_field(spec);
        case arg_id_t::type_t::integral: return append_indexed_field(arg_id.data.integral, spec);
        case arg_id_t::type_t::identifier: return append_named_field(arg_id.data.identifier, spec);
        default: BOOST_ASSERT(false); return false;  // LCOV_EXCL_LINE
        }
    }

    BOOST_ATTRIBUTE_NODISCARD
    bool append_named_field(string_view field_name, string_view format_spec)
    {
        // Find the argument
        for (const auto& arg : args_)
        {
            if (access::get_impl(arg).name == field_name)
            {
                do_field(arg, format_spec);
                return true;
            }
        }

        // Not found
        ctx_.add_error(client_errc::format_arg_not_found);
        return false;
    }

    BOOST_ATTRIBUTE_NODISCARD
    bool append_indexed_field(int index, string_view format_spec)
    {
        if (uses_auto_ids())
        {
            ctx_.add_error(client_errc::format_string_manual_auto_mix);
            return false;
        }
        next_arg_id_ = -1;
        return do_indexed_field(index, format_spec);
    }

    BOOST_ATTRIBUTE_NODISCARD
    bool append_auto_field(string_view format_spec)
    {
        if (uses_explicit_ids())
        {
            ctx_.add_error(client_errc::format_string_manual_auto_mix);
            return false;
        }
        return do_indexed_field(next_arg_id_++, format_spec);
    }

public:
    format_state(format_context_base& ctx, span<const format_arg> args) noexcept : ctx_(ctx), args_(args) {}

    void format(string_view format_str)
    {
        // We can use operator++ when we know a character is ASCII. Some charsets
        // allow ASCII continuation bytes, so we need to skip the entire character otherwise
        auto cur_begin = format_str.data();
        auto it = format_str.data();
        auto end = format_str.data() + format_str.size();
        while (it != end)
        {
            if (*it == '{')
            {
                // May be a replacement field or a literal brace. In any case, dump accumulated output
                ctx_.impl_.output.append({cur_begin, it});
                ++it;

                if (it == end)
                {
                    // If the string ends here, it's en error
                    ctx_.add_error(client_errc::format_string_invalid_syntax);
                    return;
                }
                else if (*it == '{')
                {
                    // A double brace is the escaped form of '{'
                    ctx_.append_raw("{");
                    ++it;
                }
                else
                {
                    // It's a replacement field. Process it
                    if (!parse_field(it, end))
                        return;
                }
                cur_begin = it;
            }
            else if (*it == '}')
            {
                // A lonely } is only legal as a escape curly brace (i.e. }})
                ctx_.impl_.output.append({cur_begin, it});
                ++it;
                if (it == end || *it != '}')
                {
                    ctx_.add_error(client_errc::format_string_invalid_syntax);
                    return;
                }
                ctx_.impl_.output.append("}");
                ++it;
                cur_begin = it;
            }
            else
            {
                if (!advance(it, end))
                    return;
            }
        }

        // Dump any remaining SQL
        ctx_.impl_.output.append({cur_begin, end});
    }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

void boost::mysql::format_context_base::format_arg(detail::formattable_ref_impl arg, string_view format_spec)
{
    switch (arg.type)
    {
    case detail::formattable_ref_impl::type_t::field:
        detail::append_field_view(arg.data.fv, format_spec, false, *this);
        break;
    case detail::formattable_ref_impl::type_t::field_with_specs:
        detail::append_field_view(arg.data.fv, format_spec, true, *this);
        break;
    case detail::formattable_ref_impl::type_t::fn_and_ptr:
        if (!arg.data.custom.format_fn(arg.data.custom.obj, format_spec.begin(), format_spec.end(), *this))
        {
            add_error(client_errc::format_string_invalid_specifier);
        }
        break;
    default: BOOST_ASSERT(false);  // LCOV_EXCL_LINE
    }
}

void boost::mysql::detail::vformat_sql_to(
    format_context_base& ctx,
    constant_string_view format_str,
    span<const format_arg> args
)
{
    detail::format_state(ctx, args).format(format_str.get());
}

std::string boost::mysql::format_sql(
    format_options opts,
    constant_string_view format_str,
    std::initializer_list<format_arg> args
)
{
    format_context ctx(opts);
    format_sql_to(ctx, format_str, args);
    return std::move(ctx).get().value();
}

std::pair<bool, boost::mysql::string_view> boost::mysql::detail::parse_range_specifiers(
    const char* spec_begin,
    const char* spec_end
)
{
    // range_format_spec ::=  [":" [underlying_spec]]
    // Example: {::i} => format an array of strings as identifiers

    // Empty: no specifiers
    if (spec_begin == spec_end)
        return {true, {}};

    // If the first character is not a ':', the spec is invalid.
    if (*spec_begin != ':')
        return {false, {}};
    ++spec_begin;

    // Return the rest of the range
    return {
        true,
        {spec_begin, spec_end}
    };
}

#endif
