//
// Copyright (c) 2022 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2022 Alan de Freitas (alandefreitas@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/url
//

#ifndef BOOST_URL_DETAIL_URL_IMPL_HPP
#define BOOST_URL_DETAIL_URL_IMPL_HPP

#include <boost/url/host_type.hpp>
#include <boost/url/pct_string_view.hpp>
#include <boost/url/scheme.hpp>
#include <boost/core/detail/string_view.hpp>
#include <boost/url/detail/parts_base.hpp>
#include <boost/assert.hpp>
#include <cstddef>
#include <cstdint>

namespace boost {
namespace urls {

class url_view;
class authority_view;

namespace detail {

constexpr char const* const empty_c_str_ = "";

// This is the private 'guts' of a
// url_view, exposed so different parts
// of the implementation can work on it.
// It stores the offsets and properties of
// a URL string stored elsewhere and pointed
// to by cs_.
struct BOOST_SYMBOL_VISIBLE url_impl : parts_base
{
    using size_type = std::uint32_t;

    static_assert(
        BOOST_URL_MAX_SIZE <= UINT32_MAX,
        "BOOST_URL_MAX_SIZE exceeds 32-bit url_impl capacity");

    static
    constexpr
    std::size_t const zero_ = 0;

    // never nullptr
    char const* cs_ = empty_c_str_;

    size_type offset_[id_end + 1] = {};
    size_type decoded_[id_end] = {};
    size_type nseg_ = 0;
    size_type nparam_ = 0;
    unsigned char ip_addr_[16] = {};
    // VFALCO don't we need a bool?
    std::uint16_t port_number_ = 0;
    host_type host_type_ =
        urls::host_type::none;
    scheme scheme_ =
        urls::scheme::none;

    from from_ = from::string;

    BOOST_URL_CXX14_CONSTEXPR
    url_impl(
        from b) noexcept
        : from_(b)
    {
    }

    BOOST_URL_CXX20_CONSTEXPR std::size_t len(int, int) const noexcept;
    BOOST_URL_CXX20_CONSTEXPR std::size_t len(int) const noexcept;
    BOOST_URL_CXX20_CONSTEXPR std::size_t offset(int) const noexcept;
    BOOST_URL_CXX20_CONSTEXPR core::string_view get(int) const noexcept;
    BOOST_URL_CXX20_CONSTEXPR core::string_view get(int, int) const noexcept;
    BOOST_URL_CXX20_CONSTEXPR pct_string_view pct_get(int) const noexcept;
    BOOST_URL_CXX20_CONSTEXPR pct_string_view pct_get(int, int) const noexcept;
    BOOST_URL_CXX20_CONSTEXPR void set_size(int, std::size_t) noexcept;
    BOOST_URL_CXX20_CONSTEXPR void split(int, std::size_t) noexcept;
    BOOST_URL_CXX20_CONSTEXPR void adjust_right(int first, int last, std::size_t n) noexcept;
    BOOST_URL_CXX20_CONSTEXPR void adjust_left(int first, int last, std::size_t n) noexcept;
    BOOST_URL_CXX20_CONSTEXPR void collapse(int, int, std::size_t) noexcept;

    BOOST_URL_CXX20_CONSTEXPR void apply_scheme(core::string_view) noexcept;
    BOOST_URL_CXX20_CONSTEXPR void apply_userinfo(pct_string_view const&,
        pct_string_view const*) noexcept;
    BOOST_URL_CXX20_CONSTEXPR void apply_host(host_type, pct_string_view,
        unsigned char const*) noexcept;
    BOOST_URL_CXX20_CONSTEXPR void apply_port(core::string_view, unsigned short) noexcept;
    BOOST_URL_CXX20_CONSTEXPR void apply_authority(url_impl const&) noexcept;
    BOOST_URL_CXX20_CONSTEXPR void apply_path(pct_string_view, std::size_t) noexcept;
    BOOST_URL_CXX20_CONSTEXPR void apply_query(pct_string_view, std::size_t) noexcept;
    BOOST_URL_CXX20_CONSTEXPR void apply_frag(pct_string_view) noexcept;
};

// url_impl stores 32-bit sizes; centralize narrowing with checks.
BOOST_URL_CXX14_CONSTEXPR_OR_INLINE
url_impl::size_type
to_size_type(std::size_t n) noexcept
{
    BOOST_ASSERT(n <= BOOST_URL_MAX_SIZE);
    BOOST_ASSERT(n <= UINT32_MAX);
    return static_cast<url_impl::size_type>(n);
}

BOOST_URL_CXX14_CONSTEXPR_OR_INLINE
url_impl::size_type
to_size_type(std::ptrdiff_t n) noexcept
{
    BOOST_ASSERT(n >= 0);
    return to_size_type(
        static_cast<std::size_t>(n));
}

//------------------------------------------------

// this allows a path to come from a
// url_impl or a separate core::string_view
//
// Header-only and BOOST_SYMBOL_VISIBLE since all
// members are inline. Containing classes must also be
// BOOST_SYMBOL_VISIBLE to avoid C4251 on MSVC.
class BOOST_SYMBOL_VISIBLE path_ref
    : private parts_base
{
    url_impl const* impl_ = nullptr;
    char const* data_ = nullptr;
    std::size_t size_ = 0;
    std::size_t nseg_ = 0;
    std::size_t dn_ = 0;

public:
    path_ref() noexcept = default;
    path_ref(url_impl const& impl) noexcept;
    path_ref(core::string_view,
        std::size_t, std::size_t) noexcept;
    pct_string_view buffer() const noexcept;
    std::size_t size() const noexcept;
    char const* data() const noexcept;
    char const* end() const noexcept;
    std::size_t nseg() const noexcept;
    std::size_t decoded_size() const noexcept;

    bool
    alias_of(
        url_impl const& impl) const noexcept
    {
        return impl_ == &impl;
    }

    bool
    alias_of(
        path_ref const& ref) const noexcept
    {
        if(impl_)
            return impl_ == ref.impl_;
        BOOST_ASSERT(data_ != ref.data_ || (
            size_ == ref.size_ &&
            nseg_ == ref.nseg_ &&
            dn_ == ref.dn_));
        return data_ == ref.data_;
    }
};

//------------------------------------------------

// This class represents a query string, which
// can originate from either an url_impl object
// or an independent core::string_view.
class BOOST_SYMBOL_VISIBLE query_ref
    : private parts_base
{
    url_impl const* impl_ = nullptr;
    char const* data_ = nullptr;
    std::size_t size_ = 0;
    std::size_t nparam_ = 0;
    std::size_t dn_ = 0;
    bool question_mark_ = false;

public:
    query_ref(
        core::string_view s,      // buffer, no '?'
        std::size_t dn,     // decoded size
        std::size_t nparam
            ) noexcept;
    query_ref() = default;
    query_ref(url_impl const& impl) noexcept;
    pct_string_view buffer() const noexcept;
    std::size_t size() const noexcept; // with '?'
    char const* begin() const noexcept; // no '?'
    char const* end() const noexcept;
    std::size_t nparam() const noexcept;

    bool
    alias_of(
        url_impl const& impl) const noexcept
    {
        return impl_ == &impl;
    }

    bool
    alias_of(
        query_ref const& ref) const noexcept
    {
        if(impl_)
            return impl_ == ref.impl_;
        BOOST_ASSERT(data_ != ref.data_ || (
            size_ == ref.size_ &&
            nparam_ == ref.nparam_ &&
            dn_ == ref.dn_));
        return data_ == ref.data_;
    }
};

} // detail

} // urls
} // boost

#include <boost/url/detail/impl/url_impl.hpp>

#endif
