//
// Copyright (c) 2022 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/url
//

#ifndef BOOST_URL_DETAIL_IMPL_URL_IMPL_HPP
#define BOOST_URL_DETAIL_IMPL_URL_IMPL_HPP

#include <boost/url/detail/config.hpp>
#include <boost/url/detail/memcpy.hpp>
#include <boost/url/detail/path.hpp>
#include <boost/url/detail/url_impl.hpp>
#include <boost/url/scheme.hpp>
#include <boost/assert.hpp>
#include <cstring>

namespace boost {
namespace urls {
namespace detail {

//------------------------------------------------
//
// url_impl
//
//------------------------------------------------

BOOST_URL_CXX20_CONSTEXPR_OR_INLINE
void
url_impl::
apply_scheme(
    core::string_view s) noexcept
{
    scheme_ = string_to_scheme(s);
    set_size(id_scheme, s.size() + 1);
}

BOOST_URL_CXX20_CONSTEXPR_OR_INLINE
void
url_impl::
apply_userinfo(
    pct_string_view const& user,
    pct_string_view const* pass) noexcept
{
    // this function is for
    // authority_view_rule only
    BOOST_ASSERT(from_ == from::authority);

    // userinfo
    set_size(id_user, user.size());
    decoded_[id_user] =
        detail::to_size_type(
            user.decoded_size());
    if(pass)
    {
        set_size(id_pass,
            pass->size() + 2);
        decoded_[id_pass] =
            detail::to_size_type(
                pass->decoded_size());
    }
    else
    {
        // trailing '@'
        set_size(id_pass, 1 );
    }
}

BOOST_URL_CXX20_CONSTEXPR_OR_INLINE
void
url_impl::
apply_host(
    host_type ht,
    pct_string_view s,
    unsigned char const* addr) noexcept
{
    // this function is for
    // authority_view_rule only
    BOOST_ASSERT(from_ == from::authority);

    // host, port
    host_type_ = ht;
    set_size(id_host, s.size());
    decoded_[id_host] =
        detail::to_size_type(
            s.decoded_size());
    detail::memcpy(
        ip_addr_,
        addr,
        sizeof(ip_addr_));
}

BOOST_URL_CXX20_CONSTEXPR_OR_INLINE
void
url_impl::
apply_port(
    core::string_view s,
    unsigned short pn) noexcept
{
    // this function is for
    // authority_view_rule only
    BOOST_ASSERT(from_ == from::authority);

    port_number_ = pn;
    set_size(id_port, 1 + s.size());
}

BOOST_URL_CXX20_CONSTEXPR_OR_INLINE
void
url_impl::
apply_authority(
    url_impl const& a) noexcept
{
    BOOST_ASSERT(from_ != from::authority);

    // userinfo
    set_size(id_user,
        a.len(id_user) +
        (from_ == from::authority ? 0 : 2));
    set_size(id_pass, a.len(id_pass));
    decoded_[id_user] = a.decoded_[id_user];
    decoded_[id_pass] = a.decoded_[id_pass];

    // host, port
    host_type_ = a.host_type_;
    port_number_ = a.port_number_;
    set_size(id_host, a.len(id_host));
    set_size(id_port, a.len(id_port));
    detail::memcpy(
        ip_addr_,
        a.ip_addr_,
        sizeof(ip_addr_));
    decoded_[id_host] = a.decoded_[id_host];
}

BOOST_URL_CXX20_CONSTEXPR_OR_INLINE
void
url_impl::
apply_path(
    pct_string_view s,
    std::size_t nseg) noexcept
{
    set_size(id_path, s.size());
    decoded_[id_path] =
        detail::to_size_type(
            s.decoded_size());
    nseg_ = detail::to_size_type(
        detail::path_segments(s, nseg));
}

BOOST_URL_CXX20_CONSTEXPR_OR_INLINE
void
url_impl::
apply_query(
    pct_string_view s,
    std::size_t n) noexcept
{
    nparam_ = detail::to_size_type(n);
    set_size(id_query, 1 + s.size());
    decoded_[id_query] =
        detail::to_size_type(
            s.decoded_size());
}

BOOST_URL_CXX20_CONSTEXPR_OR_INLINE
void
url_impl::
apply_frag(
    pct_string_view s) noexcept
{
    set_size(id_frag, s.size() + 1);
    decoded_[id_frag] =
        detail::to_size_type(
            s.decoded_size());
}

// return length of [first, last)
BOOST_URL_CXX20_CONSTEXPR_OR_INLINE
std::size_t
url_impl::
len(
    int first,
    int last) const noexcept
{
    BOOST_ASSERT(first <= last);
    BOOST_ASSERT(last <= id_end);
    return offset(last) - offset(first);
}

// return length of part
BOOST_URL_CXX20_CONSTEXPR_OR_INLINE
std::size_t
url_impl::
len(int id) const noexcept
{
    return id == id_end
        ? zero_
        : ( offset(id + 1) -
            offset(id) );
}

// return offset of id
BOOST_URL_CXX20_CONSTEXPR_OR_INLINE
std::size_t
url_impl::
offset(int id) const noexcept
{
    return
        id == id_scheme
        ? zero_
        : offset_[id];
}

// return id as string
BOOST_URL_CXX20_CONSTEXPR_OR_INLINE
core::string_view
url_impl::
get(int id) const noexcept
{
    return {
        cs_ + offset(id), len(id) };
}

// return [first, last) as string
BOOST_URL_CXX20_CONSTEXPR_OR_INLINE
core::string_view
url_impl::
get(int first,
    int last) const noexcept
{
    return { cs_ + offset(first),
        offset(last) - offset(first) };
}

// return id as pct-string
BOOST_URL_CXX20_CONSTEXPR_OR_INLINE
pct_string_view
url_impl::
pct_get(
    int id) const noexcept
{
    return make_pct_string_view_unsafe(
        cs_ + offset(id),
        len(id),
        decoded_[id]);
}

// return [first, last) as pct-string
BOOST_URL_CXX20_CONSTEXPR_OR_INLINE
pct_string_view
url_impl::
pct_get(
    int first,
    int last) const noexcept
{
    auto const pos = offset(first);
    std::size_t n = 0;
    for(auto i = first; i < last;)
        n += decoded_[i++];
    return make_pct_string_view_unsafe(
        cs_ + pos,
        offset(last) - pos,
        n);
}

//------------------------------------------------

// change id to size n
BOOST_URL_CXX20_CONSTEXPR_OR_INLINE
void
url_impl::
set_size(
    int id,
    std::size_t n) noexcept
{
    auto const cur = len(id);
    if(n >= cur)
    {
        auto const d = n - cur;
        for(auto i = id + 1;
            i <= id_end; ++i)
            offset_[i] += detail::to_size_type(d);
        return;
    }
    auto const d = cur - n;
    for(auto i = id + 1;
        i <= id_end; ++i)
        offset_[i] -= detail::to_size_type(d);
}

// trim id to size n,
// moving excess into id+1
BOOST_URL_CXX20_CONSTEXPR_OR_INLINE
void
url_impl::
split(
    int id,
    std::size_t n) noexcept
{
    BOOST_ASSERT(id < id_end - 1);
    //BOOST_ASSERT(n <= len(id));
    offset_[id + 1] = detail::to_size_type(
        offset(id) + n);
}

// add n to [first, last]
BOOST_URL_CXX20_CONSTEXPR_OR_INLINE
void
url_impl::
adjust_right(
    int first,
    int last,
    std::size_t n) noexcept
{
    for(int i = first;
            i <= last; ++i)
        offset_[i] += detail::to_size_type(n);
}

// remove n from [first, last]
BOOST_URL_CXX20_CONSTEXPR_OR_INLINE
void
url_impl::
adjust_left(
    int first,
    int last,
    std::size_t n) noexcept
{
    for(int i = first;
            i <= last; ++i)
        offset_[i] -= detail::to_size_type(n);
}

// set [first, last) offset
BOOST_URL_CXX20_CONSTEXPR_OR_INLINE
void
url_impl::
collapse(
    int first,
    int last,
    std::size_t n) noexcept
{
    for(int i = first + 1;
            i < last; ++i)
        offset_[i] = detail::to_size_type(n);
}


//------------------------------------------------
//
// path_ref
//
//------------------------------------------------

inline
path_ref::
path_ref(
    url_impl const& impl) noexcept
{
    if(impl.from_ == url_impl::from::url)
    {
        impl_ = &impl;
    }
    else
    {
        core::string_view s = impl.get(id_path);
        data_ = s.data();
        size_ = s.size();
        nseg_ = impl.nseg_;
        dn_ = impl.decoded_[id_path];
    }
}

inline
path_ref::
path_ref(
    core::string_view s,
    std::size_t dn,
    std::size_t nseg) noexcept
    : data_(s.data())
    , size_(s.size())
    , nseg_(nseg)
    , dn_(dn)
{
}

inline
pct_string_view
path_ref::
buffer() const noexcept
{
    if(impl_)
        return make_pct_string_view_unsafe(
            impl_->cs_ +
                impl_->offset(id_path),
            impl_->len(id_path),
            impl_->decoded_[id_path]);
    return make_pct_string_view_unsafe(
        data_, size_, dn_);
}

inline
std::size_t
path_ref::
size() const noexcept
{
    if(impl_)
        return impl_->len(id_path);
    return size_;
}

inline
char const*
path_ref::
data() const noexcept
{
    if(impl_)
        return impl_->cs_ +
            impl_->offset(id_path);
    return data_;
}

inline
char const*
path_ref::
end() const noexcept
{
    if(impl_)
        return impl_->cs_ +
            impl_->offset(id_query);
    return data_ + size_;
}

inline
std::size_t
path_ref::
nseg() const noexcept
{
    if(impl_)
        return impl_->nseg_;
    return nseg_;
}

inline
std::size_t
path_ref::
decoded_size() const noexcept
{
    if(impl_)
        return impl_->decoded_[id_path];
    return dn_;
}

//------------------------------------------------
//
// query_ref
//
//------------------------------------------------

inline
query_ref::
query_ref(
    core::string_view s,
    std::size_t dn,
    std::size_t nparam) noexcept
    : data_(s.data())
    , size_(s.size())
    , nparam_(nparam)
    , dn_(dn)
{
}

inline
query_ref::
query_ref(
    url_impl const& impl) noexcept
{
    if(impl.from_ == url_impl::from::url)
    {
        impl_ = &impl;
    }
    else
    {
        core::string_view s = impl.get(id_query);
        if (!s.empty())
        {
            s.remove_prefix(1);
            question_mark_ = true;
        }
        data_ = s.data();
        size_ = s.size();
        nparam_ = impl.nparam_;
        dn_ = impl.decoded_[id_query];
    }
}

inline
pct_string_view
query_ref::
buffer() const noexcept
{
    if(impl_)
    {
        auto pos = impl_->offset_[id_query];
        auto pos1 = impl_->offset_[id_frag];
        if(pos < pos1)
        {
            ++pos; // no '?'
            return make_pct_string_view_unsafe(
                impl_->cs_ + pos,
                pos1 - pos,
                impl_->decoded_[id_query]);
        }
        // empty
        return make_pct_string_view_unsafe(
            impl_->cs_ + pos,
            0,
            0);
    }
    // no '?'
    return make_pct_string_view_unsafe(
        data_, size_, dn_);
}

// with '?'
inline
std::size_t
query_ref::
size() const noexcept
{
    if(impl_)
        return impl_->len(id_query);
    if(size_ > 0)
        return size_ + 1;
    return question_mark_;
}

// no '?'
inline
char const*
query_ref::
begin() const noexcept
{
    if(impl_)
    {
        // using the offset array here
        auto pos = impl_->offset_[id_query];
        auto pos1 = impl_->offset_[id_frag];
        if(pos < pos1)
            return impl_->cs_ + pos + 1; // no '?'
        // empty
        return impl_->cs_ + pos;
    }
    return data_;

}

inline
char const*
query_ref::
end() const noexcept
{
    if(impl_)
        return impl_->cs_ +
            impl_->offset(id_frag);
    return data_ + size_;
}

inline
std::size_t
query_ref::
nparam() const noexcept
{
    if(impl_)
        return impl_->nparam_;
    return nparam_;
}

} // detail
} // urls
} // boost

#endif
