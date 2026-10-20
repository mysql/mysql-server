//
// Copyright (c) 2019 Vinnie Falco (vinnie.falco@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/json
//

#ifndef BOOST_JSON_IMPL_SERIALIZER_IPP
#define BOOST_JSON_IMPL_SERIALIZER_IPP

#include <boost/core/detail/static_assert.hpp>
#include <boost/json/serializer.hpp>
#include <boost/json/detail/format.hpp>
#include <boost/json/detail/sse2.hpp>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4127) // conditional expression is constant
#endif

namespace boost {
namespace json {
namespace detail {

struct int64_formatter
{
    std::int64_t i;

    std::size_t
    operator()(char* dst) const noexcept
    {
        return format_int64(dst, i);
    }
};

struct uint64_formatter
{
    std::uint64_t u;

    std::size_t
    operator()(char* dst) const noexcept
    {
        return format_uint64(dst, u);
    }
};

struct double_formatter
{
    double d;
    bool allow_infinity_and_nan;

    std::size_t
    operator()(char* dst) const noexcept
    {
        return format_double(dst, d, allow_infinity_and_nan);
    }
};

writer::
writer(
    storage_ptr sp,
    unsigned char* buf,
    std::size_t buf_size,
    serialize_options const& opts) noexcept
    : st_(
        std::move(sp),
        buf,
        buf_size)
    , opts_(opts)
{
    // ensure room for \uXXXX escape plus one
    BOOST_CORE_STATIC_ASSERT( sizeof(buf_) >= 7 );
}

bool
BOOST_FORCEINLINE
write_buffer(writer& w, stream& ss0)
{
    local_stream ss(ss0);
    auto const n = ss.remain();
    if( n < w.cs0_.remain() )
    {
        ss.append(w.cs0_.data(), n);
        w.cs0_.skip(n);
        return w.suspend(writer::state::lit);
    }
    ss.append( w.cs0_.data(), w.cs0_.remain() );
    return true;
}

template< class F >
bool
write_buffer(writer& w, stream& ss0, F f)
{
    BOOST_ASSERT( w.st_.empty() );

    local_stream ss(ss0);
    if(BOOST_JSON_LIKELY( ss.remain() >= detail::max_number_chars ))
    {
        ss.advance( f(ss.data()) );
        return true;
    }

    w.cs0_ = { w.buf_, f(w.buf_) };
    return write_buffer(w, ss);
}

template<literals Lit>
bool
write_literal(writer& w, stream& ss)
{
    constexpr std::size_t index = literal_index(Lit);
    constexpr char const* literal = literal_strings[index];
    constexpr std::size_t sz = literal_sizes[index];

    std::size_t const n = ss.remain();
    if(BOOST_JSON_LIKELY( n >= sz ))
    {
        ss.append( literal, sz );
        return true;
    }

    ss.append(literal, n);

    w.cs0_ = {literal + n, sz - n};
    return w.suspend(writer::state::lit);
}

bool
write_true(writer& w, stream& ss)
{
    return write_literal<literals::true_>(w, ss);
}

bool
write_false(writer& w, stream& ss)
{
    return write_literal<literals::false_>(w, ss);
}

bool
write_null(writer& w, stream& ss)
{
    return write_literal<literals::null>(w, ss);
}

bool
write_int64(writer& w, stream& ss0, std::int64_t i)
{
    return write_buffer( w, ss0, int64_formatter{i} );
}

bool
write_uint64(writer& w, stream& ss0, std::uint64_t u)
{
    return write_buffer( w, ss0, uint64_formatter{u} );
}

bool
write_double(writer& w, stream& ss0, double d)
{
    return write_buffer(
        w, ss0, double_formatter{d, w.opts_.allow_infinity_and_nan} );
}

bool
resume_buffer(writer& w, stream& ss0)
{
    BOOST_ASSERT( !w.st_.empty() );
    writer::state st;
    w.st_.pop(st);
    BOOST_ASSERT(st == writer::state::lit);

    return write_buffer(w, ss0);
}

template<bool StackEmpty>
bool
do_write_string(writer& w, stream& ss0)
{
    local_stream ss(ss0);
    local_const_stream cs(w.cs0_);
    if(! StackEmpty && ! w.st_.empty())
    {
        writer::state st;
        w.st_.pop(st);
        switch(st)
        {
        default:
        case writer::state::str1: goto do_str1;
        case writer::state::str2: goto do_str2;
        case writer::state::str3: goto do_str3;
        case writer::state::esc1: goto do_esc1;
        case writer::state::utf1: goto do_utf1;
        case writer::state::utf2: goto do_utf2;
        case writer::state::utf3: goto do_utf3;
        case writer::state::utf4: goto do_utf4;
        case writer::state::utf5: goto do_utf5;
        }
    }
    static constexpr char hex[] = "0123456789abcdef";
    static constexpr char esc[] =
        "uuuuuuuubtnufruuuuuuuuuuuuuuuuuu"
        "\0\0\"\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
        "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\\\0\0\0"
        "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
        "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
        "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
        "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
        "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0";

    // opening quote
do_str1:
    if(BOOST_JSON_LIKELY(ss))
        ss.append('\x22'); // '"'
    else
        return w.suspend(writer::state::str1);

    // fast loop,
    // copy unescaped
do_str2:
    if(BOOST_JSON_LIKELY(ss))
    {
        std::size_t n = cs.remain();
        if(BOOST_JSON_LIKELY(n > 0))
        {
            if(ss.remain() > n)
                n = detail::count_unescaped(
                    cs.data(), n);
            else
                n = detail::count_unescaped(
                    cs.data(), ss.remain());
            if(n > 0)
            {
                ss.append(cs.data(), n);
                cs.skip(n);
                if(! ss)
                    return w.suspend(writer::state::str2);
            }
        }
        else
        {
            ss.append('\x22'); // '"'
            return true;
        }
    }
    else
    {
        return w.suspend(writer::state::str2);
    }

    // slow loop,
    // handle escapes
do_str3:
    while(BOOST_JSON_LIKELY(ss))
    {
        if(BOOST_JSON_LIKELY(cs))
        {
            auto const ch = *cs;
            auto const c = esc[static_cast<
                unsigned char>(ch)];
            ++cs;
            if(! c)
            {
                ss.append(ch);
            }
            else if(c != 'u')
            {
                ss.append('\\');
                if(BOOST_JSON_LIKELY(ss))
                {
                    ss.append(c);
                }
                else
                {
                    w.buf_[0] = c;
                    return w.suspend(
                        writer::state::esc1);
                }
            }
            else
            {
                if(BOOST_JSON_LIKELY(
                    ss.remain() >= 6))
                {
                    ss.append("\\u00", 4);
                    ss.append(hex[static_cast<
                        unsigned char>(ch) >> 4]);
                    ss.append(hex[static_cast<
                        unsigned char>(ch) & 15]);
                }
                else
                {
                    ss.append('\\');
                    w.buf_[0] = hex[static_cast<
                        unsigned char>(ch) >> 4];
                    w.buf_[1] = hex[static_cast<
                        unsigned char>(ch) & 15];
                    goto do_utf1;
                }
            }
        }
        else
        {
            ss.append('\x22'); // '"'
            return true;
        }
    }
    return w.suspend(writer::state::str3);

do_esc1:
    BOOST_ASSERT(ss);
    ss.append(w.buf_[0]);
    goto do_str3;

do_utf1:
    if(BOOST_JSON_LIKELY(ss))
        ss.append('u');
    else
        return w.suspend(writer::state::utf1);
do_utf2:
    if(BOOST_JSON_LIKELY(ss))
        ss.append('0');
    else
        return w.suspend(writer::state::utf2);
do_utf3:
    if(BOOST_JSON_LIKELY(ss))
        ss.append('0');
    else
        return w.suspend(writer::state::utf3);
do_utf4:
    if(BOOST_JSON_LIKELY(ss))
        ss.append(w.buf_[0]);
    else
        return w.suspend(writer::state::utf4);
do_utf5:
    if(BOOST_JSON_LIKELY(ss))
        ss.append(w.buf_[1]);
    else
        return w.suspend(writer::state::utf5);
    goto do_str3;
}

bool
write_string(writer& w, stream& ss0)
{
    return do_write_string<true>(w, ss0);
}

bool
resume_string(writer& w, stream& ss0)
{
    return do_write_string<false>(w, ss0);
}

template<bool StackEmpty>
bool
write_value(writer& w, stream& ss);

template< class T, bool StackEmpty >
BOOST_FORCEINLINE
bool
write_impl(no_conversion_tag, writer& w, stream& ss)
{
    return write_value<StackEmpty>(w, ss);
}

template<bool StackEmpty>
bool
write_array(writer& w, stream& ss)
{
    return write_impl<array, StackEmpty>(sequence_conversion_tag(), w, ss);
}

template<bool StackEmpty>
bool
write_object(writer& w, stream& ss)
{
    return write_impl<object, StackEmpty>(map_like_conversion_tag(), w, ss);
}

template<bool StackEmpty>
bool
write_value(writer& w, stream& ss)
{
    if(StackEmpty || w.st_.empty())
    {
        BOOST_ASSERT( w.p_ );
        auto const pv = reinterpret_cast<value const*>(w.p_);
        switch(pv->kind())
        {
        default:
        case kind::object:
            w.p_ = &pv->get_object();
            return write_object<true>(w, ss);

        case kind::array:
            w.p_ = &pv->get_array();
            return write_array<true>(w, ss);

        case kind::string:
        {
            auto const& js = pv->get_string();
            w.cs0_ = { js.data(), js.size() };
            return do_write_string<true>(w, ss);
        }

        case kind::int64:
            return write_int64( w, ss, pv->get_int64() );
        case kind::uint64:
            return write_uint64( w, ss, pv->get_uint64() );
        case kind::double_:
            return write_double( w, ss, pv->get_double() );

        case kind::bool_:
            if( pv->get_bool() )
                return write_true(w, ss);
            else
                return write_false(w, ss);

        case kind::null:
            return write_null(w, ss);
        }
    }
    else
    {
        writer::state st;
        w.st_.peek(st);
        switch(st)
        {
        default:
        case writer::state::lit:
            return resume_buffer(w, ss);

        case writer::state::str1: case writer::state::str2:
        case writer::state::str3: case writer::state::esc1:
        case writer::state::utf1: case writer::state::utf2:
        case writer::state::utf3: case writer::state::utf4:
        case writer::state::utf5:
            return do_write_string<false>(w, ss);

        case writer::state::arr1: case writer::state::arr2:
        case writer::state::arr3: case writer::state::arr4:
            return write_array<StackEmpty>(w, ss);

        case writer::state::obj1: case writer::state::obj2:
        case writer::state::obj3: case writer::state::obj4:
        case writer::state::obj5: case writer::state::obj6:
            return write_object<StackEmpty>(w, ss);
        }
    }
}

} // namespace detail

serializer::
serializer(serialize_options const& opts) noexcept
    : serializer({}, nullptr, 0, opts)
{}

serializer::
serializer(
    storage_ptr sp,
    unsigned char* buf,
    std::size_t buf_size,
    serialize_options const& opts) noexcept
    : detail::writer(std::move(sp), buf, buf_size, opts)
{}

void
serializer::
reset(value const* p) noexcept
{
    p_ = p;
    fn0_ = &detail::write_value<true>;
    fn1_ = &detail::write_value<false>;
    st_.clear();
    done_ = false;
}

void
serializer::
reset(array const* p) noexcept
{
    p_ = p;
    fn0_ = &detail::write_array<true>;
    fn1_ = &detail::write_array<false>;
    st_.clear();
    done_ = false;
}

void
serializer::
reset(object const* p) noexcept
{
    p_ = p;
    fn0_ = &detail::write_object<true>;
    fn1_ = &detail::write_object<false>;
    st_.clear();
    done_ = false;
}

void
serializer::
reset(string const* p) noexcept
{
    cs0_ = { p->data(), p->size() };
    fn0_ = &detail::do_write_string<true>;
    fn1_ = &detail::do_write_string<false>;
    st_.clear();
    done_ = false;
}

void
serializer::
reset(string_view sv) noexcept
{
    cs0_ = { sv.data(), sv.size() };
    fn0_ = &detail::do_write_string<true>;
    fn1_ = &detail::do_write_string<false>;
    st_.clear();
    done_ = false;
}

void
serializer::reset(std::nullptr_t) noexcept
{
    p_ = nullptr;
    fn0_ = &detail::write_impl<std::nullptr_t, true>;
    fn1_ = &detail::write_impl<std::nullptr_t, false>;
    st_.clear();
    done_ = false;
}

string_view
serializer::
read(char* dest, std::size_t size)
{
    if( !fn0_ )
        reset(nullptr);

    if(BOOST_JSON_UNLIKELY(size == 0))
        return {dest, 0};

    detail::stream ss(dest, size);
    if(st_.empty())
        fn0_(*this, ss);
    else
        fn1_(*this, ss);
    if(st_.empty())
    {
        done_ = true;
        fn0_ = nullptr;
        p_ = nullptr;
    }
    return string_view(
        dest, ss.used(dest));
}

} // namespace json
} // namespace boost

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#endif
