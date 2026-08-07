//
// Copyright (c) 2019 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/url
//

#ifndef BOOST_URL_IMPL_AUTHORITY_VIEW_HPP
#define BOOST_URL_IMPL_AUTHORITY_VIEW_HPP

#include <boost/url/detail/config.hpp>
#include <boost/url/detail/memcpy.hpp>
#include <boost/url/grammar/parse.hpp>
#include <boost/url/rfc/authority_rule.hpp>

namespace boost {
namespace urls {

//------------------------------------------------

namespace detail {

// Forward declarations for normalize functions
// defined in src/detail/normalize.cpp
BOOST_URL_DECL
int
compare_encoded(
    core::string_view lhs,
    core::string_view rhs) noexcept;

BOOST_URL_DECL
int
ci_compare_encoded(
    core::string_view lhs,
    core::string_view rhs) noexcept;

BOOST_URL_DECL
int
compare(
    core::string_view lhs,
    core::string_view rhs) noexcept;

} // detail

//------------------------------------------------
//
// Special Members
//
//------------------------------------------------

BOOST_URL_CXX20_CONSTEXPR_OR_INLINE
authority_view::
authority_view(
    core::string_view s)
    : authority_view(
        parse_authority(s
            ).value(BOOST_URL_POS))
{
}

//------------------------------------------------
//
// Userinfo
//
//------------------------------------------------

BOOST_URL_CXX20_CONSTEXPR_OR_INLINE
bool
authority_view::
has_userinfo() const noexcept
{
    auto n = u_.len(id_pass);
    if(n == 0)
        return false;
    BOOST_ASSERT(u_.get(
        id_pass).ends_with('@'));
    return true;
}

BOOST_URL_CXX20_CONSTEXPR_OR_INLINE
pct_string_view
authority_view::
encoded_userinfo() const noexcept
{
    auto s = u_.get(
        id_user, id_host);
    if(s.empty())
        return s;
    BOOST_ASSERT(
        s.ends_with('@'));
    s.remove_suffix(1);
    return make_pct_string_view_unsafe(
        s.data(),
        s.size(),
        u_.decoded_[id_user] +
            u_.decoded_[id_pass] +
            has_password());
}

BOOST_URL_CXX20_CONSTEXPR_OR_INLINE
pct_string_view
authority_view::
encoded_user() const noexcept
{
    auto s = u_.get(id_user);
    return make_pct_string_view_unsafe(
        s.data(),
        s.size(),
        u_.decoded_[id_user]);
}

BOOST_URL_CXX20_CONSTEXPR_OR_INLINE
bool
authority_view::
has_password() const noexcept
{
    auto const n = u_.len(id_pass);
    if(n > 1)
    {
        BOOST_ASSERT(u_.get(id_pass
            ).starts_with(':'));
        BOOST_ASSERT(u_.get(id_pass
            ).ends_with('@'));
        return true;
    }
    BOOST_ASSERT(n == 0 || u_.get(
        id_pass).ends_with('@'));
    return false;
}

BOOST_URL_CXX20_CONSTEXPR_OR_INLINE
pct_string_view
authority_view::
encoded_password() const noexcept
{
    auto s = u_.get(id_pass);
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
        u_.decoded_[id_pass]);
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

BOOST_URL_CXX20_CONSTEXPR_OR_INLINE
pct_string_view
authority_view::
encoded_host() const noexcept
{
    return u_.pct_get(id_host);
}

BOOST_URL_CXX20_CONSTEXPR_OR_INLINE
pct_string_view
authority_view::
encoded_host_address() const noexcept
{
    core::string_view s = u_.get(id_host);
    std::size_t n;
    switch(u_.host_type_)
    {
    case urls::host_type::name:
    case urls::host_type::ipv4:
        n = u_.decoded_[id_host];
        break;

    case urls::host_type::ipv6:
    case urls::host_type::ipvfuture:
    {
        BOOST_ASSERT(
            u_.decoded_[id_host] ==
                s.size());
        BOOST_ASSERT(s.size() >= 2);
        BOOST_ASSERT(s.front() == '[');
        BOOST_ASSERT(s.back() == ']');
        s = s.substr(1, s.size() - 2);
        n = u_.decoded_[id_host] - 2;
        break;
    }
    // LCOV_EXCL_START
    default:
    case urls::host_type::none:
        /*
         * This condition is for correctness
         * only.
         * This should never happen, because
         * the `host_rule` will set the host
         * type to `name` when it's empty.
         * This is correct because `reg-name`
         * accepts empty strings.
         */
        BOOST_ASSERT(s.empty());
        n = 0;
        break;
    // LCOV_EXCL_STOP
    }
    return make_pct_string_view_unsafe(
        s.data(), s.size(), n);
}

BOOST_URL_CXX20_CONSTEXPR_OR_INLINE
ipv4_address
authority_view::
host_ipv4_address() const noexcept
{
    if(u_.host_type_ !=
            urls::host_type::ipv4)
        return {};
    ipv4_address::bytes_type b{{}};
    detail::memcpy(
        &b[0], &u_.ip_addr_[0], b.size());
    return urls::ipv4_address(b);
}

BOOST_URL_CXX20_CONSTEXPR_OR_INLINE
ipv6_address
authority_view::
host_ipv6_address() const noexcept
{
    if(u_.host_type_ !=
            urls::host_type::ipv6)
        return {};
    ipv6_address::bytes_type b{{}};
    detail::memcpy(
        &b[0], &u_.ip_addr_[0], b.size());
    return urls::ipv6_address(b);
}

BOOST_URL_CXX20_CONSTEXPR_OR_INLINE
core::string_view
authority_view::
host_ipvfuture() const noexcept
{
    if(u_.host_type_ !=
            urls::host_type::ipvfuture)
        return {};
    core::string_view s = u_.get(id_host);
    BOOST_ASSERT(s.size() >= 6);
    BOOST_ASSERT(s.front() == '[');
    BOOST_ASSERT(s.back() == ']');
    s = s.substr(1, s.size() - 2);
    return s;
}

BOOST_URL_CXX20_CONSTEXPR_OR_INLINE
pct_string_view
authority_view::
encoded_host_name() const noexcept
{
    if(u_.host_type_ !=
            urls::host_type::name)
        return {};
    return u_.pct_get(id_host);
}

//------------------------------------------------
//
// Port
//
//------------------------------------------------

BOOST_URL_CXX20_CONSTEXPR_OR_INLINE
bool
authority_view::
has_port() const noexcept
{
    auto const n = u_.len(id_port);
    if(n == 0)
        return false;
    BOOST_ASSERT(
        u_.get(id_port).starts_with(':'));
    return true;
}

BOOST_URL_CXX20_CONSTEXPR_OR_INLINE
core::string_view
authority_view::
port() const noexcept
{
    auto s = u_.get(id_port);
    if(s.empty())
        return s;
    BOOST_ASSERT(has_port());
    return s.substr(1);
}

BOOST_URL_CXX20_CONSTEXPR_OR_INLINE
std::uint16_t
authority_view::
port_number() const noexcept
{
    BOOST_ASSERT(
        has_port() ||
        u_.port_number_ == 0);
    return u_.port_number_;
}

BOOST_URL_CXX20_CONSTEXPR_OR_INLINE
pct_string_view
authority_view::
encoded_host_and_port() const noexcept
{
    return u_.pct_get(id_host, id_end);
}

//------------------------------------------------
//
// Comparisons
//
//------------------------------------------------

inline
int
authority_view::
compare(const authority_view& other) const noexcept
{
    auto comp = static_cast<int>(has_userinfo()) -
        static_cast<int>(other.has_userinfo());
    if ( comp != 0 )
        return comp;

    if (has_userinfo())
    {
        comp = detail::compare_encoded(
            encoded_user(),
            other.encoded_user());
        if ( comp != 0 )
            return comp;

        comp = static_cast<int>(has_password()) -
               static_cast<int>(other.has_password());
        if ( comp != 0 )
            return comp;

        if (has_password())
        {
            comp = detail::compare_encoded(
                encoded_password(),
                other.encoded_password());
            if ( comp != 0 )
                return comp;
        }
    }

    comp = detail::ci_compare_encoded(
        encoded_host(),
        other.encoded_host());
    if ( comp != 0 )
        return comp;

    comp = static_cast<int>(has_port()) -
           static_cast<int>(other.has_port());
    if ( comp != 0 )
        return comp;

    if (has_port())
    {
        comp = detail::compare(
            port(),
            other.port());
        if ( comp != 0 )
            return comp;
    }

    return 0;
}

//------------------------------------------------

BOOST_URL_CXX20_CONSTEXPR_OR_INLINE
system::result<authority_view>
parse_authority(
    core::string_view s) noexcept
{
    return grammar::parse(s, authority_rule);
}

} // urls
} // boost

#endif
