//
// Copyright (c) 2019 Vinnie Falco (vinnie.falco@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/url
//

#ifndef BOOST_URL_IMPL_STATIC_URL_HPP
#define BOOST_URL_IMPL_STATIC_URL_HPP

#include <boost/url/detail/except.hpp>
#include <boost/assert.hpp>

namespace boost {
namespace urls {

inline
static_url_base::
static_url_base(
    char* buf,
    std::size_t cap) noexcept
{
    s_ = buf;
    cap_ = cap;
    s_[0] = '\0';
    impl_.cs_ = s_;
}

inline
static_url_base::
static_url_base(
    char* buf,
    std::size_t cap,
    core::string_view s)
    : static_url_base(buf, cap)
{
    copy(parse_uri_reference(s
        ).value(BOOST_URL_POS));
}

inline
void
static_url_base::
clear_impl() noexcept
{
    impl_ = {from::url};
    s_[0] = '\0';
    impl_.cs_ = s_;
}

inline
void
static_url_base::
reserve_impl(
    std::size_t n,
    op_t&)
{
    if(n <= cap_)
        return;
    detail::throw_length_error();
}

//----------------------------------------------------------

// LCOV_EXCL_START
inline
void
static_url_base::
cleanup(op_t&)
{
    /*
     * The cleanup function is a blank
     * override as it's unreachable
     * for static_url_base.
     *
     * `u.cleanup()` is called by `op_t` when
     * the `op_t::old` string is being replaced.
     * This never happens for `static_url_base`
     * because it always uses the same buffer.
     *
     * `url::reserve_impl` is the only function
     * that sets the `op_t::old` string but
     * `static_url_base::reserve_impl` does
     * not touch `op_t::old`.
     */
}
// LCOV_EXCL_STOP

} // urls
} // boost

#endif
