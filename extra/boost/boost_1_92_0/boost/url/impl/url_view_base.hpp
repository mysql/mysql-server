//
// Copyright (c) 2019 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2022 Alan de Freitas (alandefreitas@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/url
//

#ifndef BOOST_URL_IMPL_URL_VIEW_BASE_HPP
#define BOOST_URL_IMPL_URL_VIEW_BASE_HPP

#include <boost/url/detail/memcpy.hpp>
#include <boost/url/detail/except.hpp>
#include <boost/url/detail/fnv_1a.hpp>
#include <boost/assert.hpp>
#include <cstring>
#include <memory>

namespace boost {
namespace urls {

namespace detail {

// Forward declarations for normalize functions
// defined in src/detail/normalize.cpp
BOOST_URL_DECL
void
ci_digest(
    core::string_view s,
    fnv_1a& hasher) noexcept;

BOOST_URL_DECL
void
digest_encoded(
    core::string_view s,
    fnv_1a& hasher) noexcept;

BOOST_URL_DECL
void
ci_digest_encoded(
    core::string_view s,
    fnv_1a& hasher) noexcept;

BOOST_URL_DECL
void
normalized_path_digest(
    core::string_view str,
    bool remove_unmatched,
    fnv_1a& hasher) noexcept;

BOOST_URL_DECL
int
ci_compare(
    core::string_view lhs,
    core::string_view rhs) noexcept;

BOOST_URL_DECL
int
compare_encoded(
    core::string_view lhs,
    core::string_view rhs) noexcept;

BOOST_URL_DECL
int
compare_encoded_query(
    core::string_view lhs,
    core::string_view rhs) noexcept;

BOOST_URL_DECL
int
segments_compare(
    segments_encoded_view seg0,
    segments_encoded_view seg1) noexcept;

} // detail

//------------------------------------------------

inline
std::size_t
url_view_base::
digest(std::size_t salt) const noexcept
{
    detail::fnv_1a h(salt);
    detail::ci_digest(impl().get(id_scheme), h);
    detail::digest_encoded(impl().get(id_user), h);
    detail::digest_encoded(impl().get(id_pass), h);
    detail::ci_digest_encoded(impl().get(id_host), h);
    h.put(impl().get(id_port));
    detail::normalized_path_digest(
        impl().get(id_path), is_path_absolute(), h);
    detail::digest_encoded(impl().get(id_query), h);
    detail::digest_encoded(impl().get(id_frag), h);
    return h.digest();
}

//------------------------------------------------
//
// Scheme
//
//------------------------------------------------

inline
bool
url_view_base::
has_scheme() const noexcept
{
    auto const n = impl().len(
        id_scheme);
    if(n == 0)
        return false;
    BOOST_ASSERT(n > 1);
    BOOST_ASSERT(
        impl().get(id_scheme
            ).ends_with(':'));
    return true;
}

inline
core::string_view
url_view_base::
scheme() const noexcept
{
    auto s = impl().get(id_scheme);
    if(! s.empty())
    {
        BOOST_ASSERT(s.size() > 1);
        BOOST_ASSERT(s.ends_with(':'));
        s.remove_suffix(1);
    }
    return s;
}

inline
urls::scheme
url_view_base::
scheme_id() const noexcept
{
    return impl().scheme_;
}

//------------------------------------------------
//
// Authority
//
//------------------------------------------------

inline
authority_view
url_view_base::
authority() const noexcept
{
    detail::url_impl u(from::authority);
    u.cs_ = encoded_authority().data();
    if(has_authority())
    {
        u.set_size(id_user, impl().len(id_user) - 2);
        u.set_size(id_pass, impl().len(id_pass));
        u.set_size(id_host, impl().len(id_host));
        u.set_size(id_port, impl().len(id_port));
    }
    else
    {
        u.set_size(id_user, impl().len(id_user));
        BOOST_ASSERT(impl().len(id_pass) == 0);
        BOOST_ASSERT(impl().len(id_host) == 0);
        BOOST_ASSERT(impl().len(id_port) == 0);
    }
    u.decoded_[id_user] = impl().decoded_[id_user];
    u.decoded_[id_pass] = impl().decoded_[id_pass];
    u.decoded_[id_host] = impl().decoded_[id_host];
    detail::memcpy(
        u.ip_addr_,
        impl().ip_addr_,
        16);
    u.port_number_ = impl().port_number_;
    u.host_type_ = impl().host_type_;
    return authority_view(u);
}

inline
pct_string_view
url_view_base::
encoded_authority() const noexcept
{
    auto s = impl().get(id_user, id_path);
    if(! s.empty())
    {
        BOOST_ASSERT(has_authority());
        s.remove_prefix(2);
    }
    return make_pct_string_view_unsafe(
        s.data(),
        s.size(),
        impl().decoded_[id_user] +
            impl().decoded_[id_pass] +
            impl().decoded_[id_host] +
            impl().decoded_[id_port] +
            has_password());
}

//------------------------------------------------
//
// Userinfo
//
//------------------------------------------------

inline
bool
url_view_base::
has_userinfo() const noexcept
{
    auto n = impl().len(id_pass);
    if(n == 0)
        return false;
    BOOST_ASSERT(has_authority());
    BOOST_ASSERT(impl().get(
        id_pass).ends_with('@'));
    return true;
}

inline
bool
url_view_base::
has_password() const noexcept
{
    auto const n = impl().len(id_pass);
    if(n > 1)
    {
        BOOST_ASSERT(impl().get(id_pass
            ).starts_with(':'));
        BOOST_ASSERT(impl().get(id_pass
            ).ends_with('@'));
        return true;
    }
    BOOST_ASSERT(n == 0 || impl().get(
        id_pass).ends_with('@'));
    return false;
}

inline
pct_string_view
url_view_base::
encoded_userinfo() const noexcept
{
    auto s = impl().get(
        id_user, id_host);
    if(s.empty())
        return s;
    BOOST_ASSERT(
        has_authority());
    s.remove_prefix(2);
    if(s.empty())
        return s;
    BOOST_ASSERT(
        s.ends_with('@'));
    s.remove_suffix(1);
    return make_pct_string_view_unsafe(
        s.data(),
        s.size(),
        impl().decoded_[id_user] +
            impl().decoded_[id_pass] +
            has_password());
}

inline
pct_string_view
url_view_base::
encoded_user() const noexcept
{
    auto s = impl().get(id_user);
    if(! s.empty())
    {
        BOOST_ASSERT(
            has_authority());
        s.remove_prefix(2);
    }
    return make_pct_string_view_unsafe(
        s.data(),
        s.size(),
        impl().decoded_[id_user]);
}

inline
pct_string_view
url_view_base::
encoded_password() const noexcept
{
    auto s = impl().get(id_pass);
    switch(s.size())
    {
    case 1:
        BOOST_ASSERT(
            s.starts_with('@'));
        s.remove_prefix(1);
        BOOST_FALLTHROUGH;
    case 0:
        return make_pct_string_view_unsafe(
            s.data(), s.size(), 0);
    default:
        break;
    }
    BOOST_ASSERT(s.ends_with('@'));
    BOOST_ASSERT(s.starts_with(':'));
    return make_pct_string_view_unsafe(
        s.data() + 1,
        s.size() - 2,
        impl().decoded_[id_pass]);
}

//------------------------------------------------
//
// Host
//
//------------------------------------------------
/*
host_type       host_type()                 // ipv4, ipv6, ipvfuture, name

std::string     host()                      // return encoded_host().decode()
pct_string_view encoded_host()              // return host part, as-is
std::string     host_address()              // return encoded_host_address().decode()
pct_string_view encoded_host_address()      // ipv4, ipv6, ipvfut, or encoded name, no brackets

ipv4_address    host_ipv4_address()         // return ipv4_address or {}
ipv6_address    host_ipv6_address()         // return ipv6_address or {}
core::string_view     host_ipvfuture()            // return ipvfuture or {}
std::string     host_name()                 // return decoded name or ""
pct_string_view encoded_host_name()         // return encoded host name or ""
*/

inline
pct_string_view
url_view_base::
encoded_host() const noexcept
{
    return impl().pct_get(id_host);
}

inline
pct_string_view
url_view_base::
encoded_host_address() const noexcept
{
    core::string_view s = impl().get(id_host);
    std::size_t n;
    switch(impl().host_type_)
    {
    default:
    case urls::host_type::none:
        BOOST_ASSERT(s.empty());
        n = 0;
        break;

    case urls::host_type::name:
    case urls::host_type::ipv4:
        n = impl().decoded_[id_host];
        break;

    case urls::host_type::ipv6:
    case urls::host_type::ipvfuture:
    {
        BOOST_ASSERT(
            impl().decoded_[id_host] ==
                s.size() ||
            !this->encoded_zone_id().empty());
        BOOST_ASSERT(s.size() >= 2);
        BOOST_ASSERT(s.front() == '[');
        BOOST_ASSERT(s.back() == ']');
        s = s.substr(1, s.size() - 2);
        n = impl().decoded_[id_host] - 2;
        break;
    }
    }
    return make_pct_string_view_unsafe(
        s.data(),
        s.size(),
        n);
}

inline
urls::ipv4_address
url_view_base::
host_ipv4_address() const noexcept
{
    if(impl().host_type_ !=
            urls::host_type::ipv4)
        return {};
    ipv4_address::bytes_type b{{}};
    std::memcpy(
        &b[0], &impl().ip_addr_[0], b.size());
    return urls::ipv4_address(b);
}

inline
urls::ipv6_address
url_view_base::
host_ipv6_address() const noexcept
{
    if(impl().host_type_ !=
            urls::host_type::ipv6)
        return {};
    ipv6_address::bytes_type b{{}};
    std::memcpy(
        &b[0], &impl().ip_addr_[0], b.size());
    return {b};
}

inline
core::string_view
url_view_base::
host_ipvfuture() const noexcept
{
    if(impl().host_type_ !=
            urls::host_type::ipvfuture)
        return {};
    core::string_view s = impl().get(id_host);
    BOOST_ASSERT(s.size() >= 6);
    BOOST_ASSERT(s.front() == '[');
    BOOST_ASSERT(s.back() == ']');
    s = s.substr(1, s.size() - 2);
    return s;
}

inline
pct_string_view
url_view_base::
encoded_host_name() const noexcept
{
    if(impl().host_type_ !=
            urls::host_type::name)
        return {};
    core::string_view s = impl().get(id_host);
    return make_pct_string_view_unsafe(
        s.data(),
        s.size(),
        impl().decoded_[id_host]);
}

inline
pct_string_view
url_view_base::
encoded_zone_id() const noexcept
{
    if(impl().host_type_ !=
        urls::host_type::ipv6)
        return {};
    core::string_view s = impl().get(id_host);
    BOOST_ASSERT(s.front() == '[');
    BOOST_ASSERT(s.back() == ']');
    s = s.substr(1, s.size() - 2);
    auto pos = s.find("%25");
    if (pos == core::string_view::npos)
        return {};
    s.remove_prefix(pos + 3);
    return *make_pct_string_view(s);
}

//------------------------------------------------

inline
bool
url_view_base::
has_port() const noexcept
{
    auto const n = impl().len(id_port);
    if(n == 0)
        return false;
    BOOST_ASSERT(
        impl().get(id_port).starts_with(':'));
    return true;
}

inline
core::string_view
url_view_base::
port() const noexcept
{
    auto s = impl().get(id_port);
    if(s.empty())
        return s;
    BOOST_ASSERT(has_port());
    return s.substr(1);
}

inline
std::uint16_t
url_view_base::
port_number() const noexcept
{
    BOOST_ASSERT(
        has_port() ||
        impl().port_number_ == 0);
    return impl().port_number_;
}

//------------------------------------------------
//
// Path
//
//------------------------------------------------

inline
pct_string_view
url_view_base::
encoded_path() const noexcept
{
    return impl().pct_get(id_path);
}

inline
segments_view
url_view_base::
segments() const noexcept
{
    return {detail::path_ref(impl())};
}

inline
segments_encoded_view
url_view_base::
encoded_segments() const noexcept
{
    return segments_encoded_view(
        detail::path_ref(impl()));
}

//------------------------------------------------
//
// Query
//
//------------------------------------------------

inline
bool
url_view_base::
has_query() const noexcept
{
    auto const n = impl().len(
        id_query);
    if(n == 0)
        return false;
    BOOST_ASSERT(
        impl().get(id_query).
            starts_with('?'));
    return true;
}

inline
pct_string_view
url_view_base::
encoded_query() const noexcept
{
    auto s = impl().get(id_query);
    if(s.empty())
        return s;
    BOOST_ASSERT(
        s.starts_with('?'));
    return s.substr(1);
}

inline
params_encoded_view
url_view_base::
encoded_params() const noexcept
{
    return params_encoded_view(impl());
}

inline
params_view
url_view_base::
params() const noexcept
{
    return params_view(
        impl(),
        encoding_opts{
            true,false,false});
}

inline
params_view
url_view_base::
params(encoding_opts opt) const noexcept
{
    return params_view(impl(), opt);
}

//------------------------------------------------
//
// Fragment
//
//------------------------------------------------

inline
bool
url_view_base::
has_fragment() const noexcept
{
    auto const n = impl().len(id_frag);
    if(n == 0)
        return false;
    BOOST_ASSERT(
        impl().get(id_frag).
            starts_with('#'));
    return true;
}

inline
pct_string_view
url_view_base::
encoded_fragment() const noexcept
{
    auto s = impl().get(id_frag);
    if(! s.empty())
    {
        BOOST_ASSERT(
            s.starts_with('#'));
        s.remove_prefix(1);
    }
    return make_pct_string_view_unsafe(
        s.data(),
        s.size(),
        impl().decoded_[id_frag]);
}

//------------------------------------------------
//
// Compound Fields
//
//------------------------------------------------

inline
pct_string_view
url_view_base::
encoded_host_and_port() const noexcept
{
    return impl().pct_get(id_host, id_path);
}

inline
pct_string_view
url_view_base::
encoded_origin() const noexcept
{
    if(impl().len(id_user) < 2)
        return {};
    return impl().get(id_scheme, id_path);
}

inline
pct_string_view
url_view_base::
encoded_resource() const noexcept
{
    auto n =
        impl().decoded_[id_path] +
        impl().decoded_[id_query] +
        impl().decoded_[id_frag];
    if(has_query())
        ++n;
    if(has_fragment())
        ++n;
    BOOST_ASSERT(pct_string_view(
        impl().get(id_path, id_end)
            ).decoded_size() == n);
    auto s = impl().get(id_path, id_end);
    return make_pct_string_view_unsafe(
        s.data(), s.size(), n);
}

inline
pct_string_view
url_view_base::
encoded_target() const noexcept
{
    auto n =
        impl().decoded_[id_path] +
        impl().decoded_[id_query];
    if(has_query())
        ++n;
    BOOST_ASSERT(pct_string_view(
        impl().get(id_path, id_frag)
            ).decoded_size() == n);
    auto s = impl().get(id_path, id_frag);
    return make_pct_string_view_unsafe(
        s.data(), s.size(), n);
}

//------------------------------------------------
//
// Comparisons
//
//------------------------------------------------

inline
int
url_view_base::
compare(const url_view_base& other) const noexcept
{
    int comp =
        static_cast<int>(has_scheme()) -
        static_cast<int>(other.has_scheme());
    if ( comp != 0 )
        return comp;

    if (has_scheme())
    {
        comp = detail::ci_compare(
            scheme(),
            other.scheme());
        if ( comp != 0 )
            return comp;
    }

    comp =
        static_cast<int>(has_authority()) -
        static_cast<int>(other.has_authority());
    if ( comp != 0 )
        return comp;

    if (has_authority())
    {
        comp = authority().compare(other.authority());
        if ( comp != 0 )
            return comp;
    }

    comp = detail::segments_compare(
        encoded_segments(),
        other.encoded_segments());
    if ( comp != 0 )
        return comp;

    comp =
        static_cast<int>(has_query()) -
        static_cast<int>(other.has_query());
    if ( comp != 0 )
        return comp;

    if (has_query())
    {
        comp = detail::compare_encoded_query(
            encoded_query(),
            other.encoded_query());
        if ( comp != 0 )
            return comp;
    }

    comp =
        static_cast<int>(has_fragment()) -
        static_cast<int>(other.has_fragment());
    if ( comp != 0 )
        return comp;

    if (has_fragment())
    {
        comp = detail::compare_encoded(
            encoded_fragment(),
            other.encoded_fragment());
        if ( comp != 0 )
            return comp;
    }

    return 0;
}

} // urls
} // boost

#endif
