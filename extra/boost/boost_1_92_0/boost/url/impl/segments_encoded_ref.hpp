//
// Copyright (c) 2019 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2022 Alan de Freitas (alandefreitas@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/url
//

#ifndef BOOST_URL_IMPL_SEGMENTS_ENCODED_REF_HPP
#define BOOST_URL_IMPL_SEGMENTS_ENCODED_REF_HPP

#include <boost/url/detail/config.hpp>
#include <boost/url/detail/segments_iter_impl.hpp>
#include <boost/url/detail/any_segments_iter.hpp>
#include <boost/url/detail/path.hpp>
#include <type_traits>

namespace boost {
namespace urls {

//------------------------------------------------
//
// Modifiers
//
//------------------------------------------------

template<class FwdIt>
void
segments_encoded_ref::
assign(
    FwdIt first, FwdIt last)
{
/*  If you get a compile error here, it
    means that the iterators you passed
    do not meet the requirements stated
    in the documentation.
*/
    static_assert(
        std::is_convertible<
            typename std::iterator_traits<
                FwdIt>::reference,
            core::string_view>::value,
        "Type requirements not met");

    u_->edit_segments(
        begin().it_,
        end().it_,
        detail::make_segments_encoded_iter(
            first, last));
}

template<class FwdIt>
auto
segments_encoded_ref::
insert(
    iterator before,
    FwdIt first,
    FwdIt last) ->
        iterator
{
/*  If you get a compile error here, it
    means that the iterators you passed
    do not meet the requirements stated
    in the documentation.
*/
    static_assert(
        std::is_convertible<
            typename std::iterator_traits<
                FwdIt>::reference,
            core::string_view>::value,
        "Type requirements not met");

    return insert(
        before,
        first,
        last,
        typename std::iterator_traits<
            FwdIt>::iterator_category{});
}

template<class FwdIt>
auto
segments_encoded_ref::
replace(
    iterator from,
    iterator to,
    FwdIt first,
    FwdIt last) ->
        iterator
{
/*  If you get a compile error here, it
    means that the iterators you passed
    do not meet the requirements stated
    in the documentation.
*/
    static_assert(
        std::is_convertible<
            typename std::iterator_traits<
                FwdIt>::reference,
            core::string_view>::value,
        "Type requirements not met");

    return u_->edit_segments(
        from.it_,
        to.it_,
        detail::make_segments_encoded_iter(
            first, last));
}

//------------------------------------------------

template<class FwdIt>
auto
segments_encoded_ref::
insert(
    iterator before,
    FwdIt first,
    FwdIt last,
    std::forward_iterator_tag) ->
        iterator
{
    return u_->edit_segments(
        before.it_,
        before.it_,
        detail::make_segments_encoded_iter(
            first, last));
}

//------------------------------------------------
//
// Special Members
//
//------------------------------------------------

inline
segments_encoded_ref::
segments_encoded_ref(
    url_base& u) noexcept
    : segments_encoded_base(
        detail::path_ref(u.impl_))
    , u_(&u)
{
}

inline
segments_encoded_ref::
operator
segments_encoded_view() const noexcept
{
    return segments_encoded_view(ref_);
}

inline
segments_encoded_ref&
segments_encoded_ref::
operator=(
    segments_encoded_ref const& other)
{
    if (!ref_.alias_of(other.ref_))
        assign(other.begin(), other.end());
    return *this;
}

inline
segments_encoded_ref&
segments_encoded_ref::
operator=(
    segments_encoded_view const& other)
{
    assign(other.begin(), other.end());
    return *this;
}

inline
segments_encoded_ref&
segments_encoded_ref::
operator=(std::initializer_list<
    pct_string_view> init)
{
    assign(init.begin(), init.end());
    return *this;
}

//------------------------------------------------
//
// Modifiers
//
//------------------------------------------------

inline
void
segments_encoded_ref::
clear() noexcept
{
    erase(begin(), end());
}

inline
void
segments_encoded_ref::
assign(
    std::initializer_list<
        pct_string_view> init)
{
    assign(init.begin(), init.end());
}

inline
auto
segments_encoded_ref::
insert(
    iterator before,
    pct_string_view s) ->
        iterator
{
    return u_->edit_segments(
        before.it_,
        before.it_,
        detail::segment_encoded_iter(s));
}

inline
auto
segments_encoded_ref::
insert(
    iterator before,
    std::initializer_list<
            pct_string_view> init) ->
        iterator
{
    return insert(
        before,
        init.begin(),
        init.end());
}

inline
auto
segments_encoded_ref::
erase(
    iterator first,
    iterator last) noexcept ->
        iterator
{
    core::string_view s;
    return u_->edit_segments(
        first.it_,
        last.it_,
        detail::make_segments_encoded_iter(
            &s, &s));
}

inline
auto
segments_encoded_ref::
replace(
    iterator pos,
    pct_string_view s) ->
        iterator
{
    return u_->edit_segments(
        pos.it_,
        std::next(pos).it_,
        detail::segment_encoded_iter(s));
}

inline
auto
segments_encoded_ref::
replace(
    iterator from,
    iterator to,
    pct_string_view s) ->
        iterator
{
    return u_->edit_segments(
        from.it_,
        to.it_,
        detail::segment_encoded_iter(s));
}

inline
auto
segments_encoded_ref::
replace(
    iterator from,
    iterator to,
    std::initializer_list<
        pct_string_view> init) ->
    iterator
{
    return replace(
        from,
        to,
        init.begin(),
        init.end());
}

inline
auto
segments_encoded_ref::
erase(
    iterator pos) noexcept ->
        iterator
{
    return erase(pos, std::next(pos));
}

inline
void
segments_encoded_ref::
push_back(
    pct_string_view s)
{
    insert(end(), s);
}

inline
void
segments_encoded_ref::
pop_back() noexcept
{
    erase(std::prev(end()));
}

} // urls
} // boost

#endif
