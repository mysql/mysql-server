//
// Copyright (c) 2019 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/url
//

#ifndef BOOST_URL_DETAIL_IMPL_ANY_PARAMS_ITER_HPP
#define BOOST_URL_DETAIL_IMPL_ANY_PARAMS_ITER_HPP

#include <boost/url/encode.hpp>
#include <boost/url/rfc/detail/charsets.hpp>
#include <boost/assert.hpp>

namespace boost {
namespace urls {
namespace detail {

/*
    When a string is transformed into a range of
    params, the empty string becomes ambiguous:
    it can be an empty range, or a range with
    one param. The value `not_empty` is used on
    construction to inform the transformation
    that the empty string should be treated as
    a one-element range. This simplifies
    edit_params().
*/

//------------------------------------------------
//
// any_params_iter
//
//------------------------------------------------

inline
any_params_iter::
~any_params_iter() noexcept = default;

//------------------------------------------------
//
// query_iter
//
//------------------------------------------------

inline
query_string_iter::
query_string_iter(
    core::string_view s,
    bool ne) noexcept
    : any_params_iter(
        s.empty() && ! ne, s)
{
    rewind();
}

inline
void
query_string_iter::
rewind() noexcept
{
    if(empty)
    {
        at_end_ = true;
        return;
    }
    p_ = s0.begin();
    if(! s0.empty())
    {
        auto pos =
            s0.find_first_of('&');
        if(pos != core::string_view::npos)
            n_ = pos;
        else
            n_ = s0.size();
    }
    else
    {
        n_ = 0;
    }
    at_end_ = false;
}

inline
bool
query_string_iter::
measure(
    std::size_t& n) noexcept
{
    if(at_end_)
        return false;
    // When interacting with the query as
    // an intact string, we do not treat
    // the plus sign as an encoded space.
    encoding_opts opt;
    opt.space_as_plus = false;
    n += encoded_size(
        core::string_view(p_, n_),
        query_chars,
        opt);
    increment();
    return true;
}

inline
void
query_string_iter::
copy(
    char*& dest,
    char const* end) noexcept
{
    BOOST_ASSERT(! at_end_);
    // When interacting with the query as
    // an intact string, we do not treat
    // the plus sign as an encoded space.
    encoding_opts opt;
    opt.space_as_plus = false;
    dest += encode_unsafe(
        dest,
        end - dest,
        core::string_view(p_, n_),
        query_chars,
        opt);
    increment();
}

inline
void
query_string_iter::
increment() noexcept
{
    p_ += n_;
    if(p_ == s0.end())
    {
        at_end_ = true;
        return;
    }
    ++p_;
    core::string_view s(p_, s0.end() - p_);
    auto pos = s.find_first_of('&');
    if(pos != core::string_view::npos)
        n_ = pos;
    else
        n_ = s.size();
}

//------------------------------------------------
//
// param_iter
//
//------------------------------------------------

inline
single_param_iter::
single_param_iter(
    param_view const& p,
    bool space_as_plus) noexcept
    : any_params_iter(
        false,
        p.key,
        p.value)
    , has_value_(p.has_value)
    , space_as_plus_(space_as_plus)
{
}

inline
void
single_param_iter::
rewind() noexcept
{
    at_end_ = false;
}

inline
bool
single_param_iter::
measure(std::size_t& n) noexcept
{
    if(at_end_)
        return false;
    encoding_opts opt;
    opt.space_as_plus = space_as_plus_;
    n += encoded_size(
        s0,
        detail::param_key_chars,
        opt);
    if(has_value_)
    {
        ++n; // '='
        n += encoded_size(
            s1,
            detail::param_value_chars,
            opt);
    }
    at_end_ = true;
    return true;
}

inline
void
single_param_iter::
copy(
    char*& dest,
    char const* end) noexcept
{
    BOOST_ASSERT(! at_end_);
    encoding_opts opt;
    opt.space_as_plus = space_as_plus_;
    dest += encode(
        dest,
        end - dest,
        s0,
        detail::param_key_chars,
        opt);
    if (has_value_)
    {
        *dest++ = '=';
        dest += encode(
            dest,
            end - dest,
            s1,
            detail::param_value_chars,
            opt);
    }
}

//------------------------------------------------
//
// params_iter_base
//
//------------------------------------------------

inline
void
params_iter_base::
measure_impl(
    std::size_t& n,
    param_view const& p) noexcept
{
    encoding_opts opt;
    opt.space_as_plus = space_as_plus_;
    n += encoded_size(
        p.key,
        detail::param_key_chars,
        opt);
    if(p.has_value)
    {
        ++n; // '='
        n += encoded_size(
            p.value,
            detail::param_value_chars,
            opt);
    }
}

inline
void
params_iter_base::
copy_impl(
    char*& dest,
    char const* end,
    param_view const& p) noexcept
{
    encoding_opts opt;
    opt.space_as_plus = space_as_plus_;
    dest += encode(
        dest,
        end - dest,
        p.key,
        detail::param_key_chars,
        opt);
    if(p.has_value)
    {
        *dest++ = '=';
        dest += encode(
            dest,
            end - dest,
            p.value,
            detail::param_value_chars,
            opt);
    }
}

//------------------------------------------------
//
// param_encoded_iter
//
//------------------------------------------------

inline
param_encoded_iter::
param_encoded_iter(
    param_pct_view const& p) noexcept
    : any_params_iter(
        false,
        p.key,
        p.value)
    , has_value_(p.has_value)
{
}

inline
void
param_encoded_iter::
rewind() noexcept
{
    at_end_ = false;
}

inline
bool
param_encoded_iter::
measure(std::size_t& n) noexcept
{
    if(at_end_)
        return false;
    n += detail::re_encoded_size_unsafe(
        s0,
        detail::param_key_chars);
    if(has_value_)
        n += detail::re_encoded_size_unsafe(
            s1,
            detail::param_value_chars) + 1; // for '='
    at_end_ = true;
    return true;
}

inline
void
param_encoded_iter::
copy(
    char*& dest,
    char const* end) noexcept
{
    detail::re_encode_unsafe(
        dest,
        end,
        s0,
        detail::param_key_chars);
    if(has_value_)
    {
        *dest++ = '=';
        detail::re_encode_unsafe(
            dest,
            end,
            s1,
            detail::param_value_chars);
    }
}


//------------------------------------------------
//
// params_encoded_iter_base
//
//------------------------------------------------

inline
void
params_encoded_iter_base::
measure_impl(
    std::size_t& n,
    param_view const& p) noexcept
{
    n += detail::re_encoded_size_unsafe(
        p.key,
        detail::param_key_chars);
    if(p.has_value)
        n += detail::re_encoded_size_unsafe(
            p.value,
            detail::param_value_chars) + 1; // for '='
}

inline
void
params_encoded_iter_base::
copy_impl(
    char*& dest,
    char const* end,
    param_view const& p) noexcept
{
    detail::re_encode_unsafe(
        dest,
        end,
        p.key,
        detail::param_key_chars);
    if(p.has_value)
    {
        *dest++ = '=';
        detail::re_encode_unsafe(
            dest,
            end,
            p.value,
            detail::param_value_chars);
    }
}

//------------------------------------------------
//
// param_value_iter
//
//------------------------------------------------

inline
void
param_value_iter::
rewind() noexcept
{
    at_end_ = false;
}

inline
bool
param_value_iter::
measure(
    std::size_t& n) noexcept
{
    if(at_end_)
        return false;
    n += nk_; // skip key
    if(has_value_)
    {
        encoding_opts opt;
        opt.space_as_plus = false;
        n += encoded_size(
            s0,
            detail::param_value_chars,
            opt) + 1; // for '='
    }
    at_end_ = true;
    return true;
}

inline
void
param_value_iter::
copy(char*& it, char const* end) noexcept
{
    it += nk_; // skip key
    if(! has_value_)
        return;
    *it++ = '=';
    encoding_opts opt;
    opt.space_as_plus = false;
    it += encode(
        it,
        end - it,
        s0,
        detail::param_value_chars,
        opt);
}

//------------------------------------------------
//
// param_encoded_value_iter
//
//------------------------------------------------

inline
void
param_encoded_value_iter::
rewind() noexcept
{
    at_end_ = false;
}

inline
bool
param_encoded_value_iter::
measure(
    std::size_t& n) noexcept
{
    if(at_end_)
        return false;
    n += nk_; // skip key
    if(has_value_)
    {
        n += detail::re_encoded_size_unsafe(
            s0,
            detail::param_value_chars) + 1; // for '='
    }
    at_end_ = true;
    return true;
}

inline
void
param_encoded_value_iter::
copy(
    char*& dest,
    char const* end) noexcept
{
    dest += nk_; // skip key
    if(! has_value_)
        return;
    *dest++ = '=';
    detail::re_encode_unsafe(
        dest,
        end,
        s0,
        detail::param_value_chars);
}

} // detail
} // urls
} // boost

#endif
