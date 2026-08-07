//
// Copyright (c) 2019 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2022 Alan de Freitas (alandefreitas@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/url
//

#ifndef BOOST_URL_IMPL_PARAMS_REF_HPP
#define BOOST_URL_IMPL_PARAMS_REF_HPP

#include <boost/url/params_view.hpp>
#include <boost/url/url_base.hpp>
#include <boost/url/detail/any_params_iter.hpp>
#include <boost/url/detail/except.hpp>
#include <boost/url/grammar/ci_string.hpp>
#include <boost/url/grammar/recycled.hpp>
#include <boost/assert.hpp>
#include <utility>

namespace boost {
namespace urls {

template<class FwdIt>
void
params_ref::
assign(FwdIt first, FwdIt last)
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
            param_view>::value,
        "Type requirements not met");

    assign(first, last,
        typename std::iterator_traits<
            FwdIt>::iterator_category{});
}

template<class FwdIt>
auto
params_ref::
append(FwdIt first, FwdIt last) ->
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
            param_view>::value,
        "Type requirements not met");

    return insert(
        end(), first, last);
}

template<class FwdIt>
auto
params_ref::
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
            param_view>::value,
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
params_ref::
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
            param_view>::value,
        "Type requirements not met");

    return iterator(
        u_->edit_params(
            from.it_, to.it_,
            detail::make_params_iter(
                first, last, opt_.space_as_plus)),
        opt_);
}

//------------------------------------------------
//
// implementation
//
//------------------------------------------------

template<class FwdIt>
void
params_ref::
assign(FwdIt first, FwdIt last,
    std::forward_iterator_tag)
{
    u_->edit_params(
        begin().it_,
        end().it_,
        detail::make_params_iter(
            first, last, opt_.space_as_plus));
}

template<class FwdIt>
auto
params_ref::
insert(
    iterator before,
    FwdIt first,
    FwdIt last,
    std::forward_iterator_tag) ->
        iterator
{
    return iterator(
        u_->edit_params(
            before.it_,
            before.it_,
            detail::make_params_iter(
                first, last, opt_.space_as_plus)),
        opt_);
}

//------------------------------------------------
//
// Special Members
//
//------------------------------------------------

inline
params_ref::
params_ref(
    url_base& u,
    encoding_opts opt) noexcept
    : params_base(u.impl_, opt)
    , u_(&u)
{
}

inline
params_ref::
params_ref(
    params_ref const& other,
    encoding_opts opt) noexcept
    : params_base(other.u_->impl_, opt)
    , u_(other.u_)
{
}

inline
auto
params_ref::
operator=(params_ref const& other) ->
    params_ref&
{
    if (!ref_.alias_of(other.ref_))
        assign(other.begin(), other.end());
    return *this;
}

inline
auto
params_ref::
operator=(std::initializer_list<
    param_view> init) ->
        params_ref&
{
    assign(init);
    return *this;
}

inline
params_ref::
operator
params_view() const noexcept
{
    return { ref_, opt_ };
}

//------------------------------------------------
//
// Modifiers
//
//------------------------------------------------

inline
void
params_ref::
clear() noexcept
{
    u_->remove_query();
}

inline
void
params_ref::
assign(
    std::initializer_list<
        param_view> init)
{
    assign(init.begin(), init.end());
}

inline
auto
params_ref::
insert(
    iterator before,
    param_view const& p) ->
        iterator
{
    return {
        u_->edit_params(
            before.it_,
            before.it_,
            detail::single_param_iter(p, opt_.space_as_plus)),
        opt_};
}

inline
auto
params_ref::
insert(
    iterator before,
    std::initializer_list<
        param_view> init) ->
    iterator
{
    return insert(
        before,
        init.begin(),
        init.end());
}

inline
auto
params_ref::
append(
    param_view const& p) ->
        iterator
{
    return insert(end(), p);
}

inline
auto
params_ref::
append(
    std::initializer_list<
        param_view> init) ->
    iterator
{
    return insert(end(), init);
}

inline
std::size_t
params_ref::
erase(
    core::string_view key,
    ignore_case_param ic) noexcept
{
    // end() can't be fully cached,
    // since erase invalidates it.
    iterator it;
    {
        auto const end_ = end();
        it = find_last(end_, key, ic);
        if(it == end_)
            return 0;
    }
    std::size_t n = 0;
    for(;;)
    {
        ++n;
        // Use it->key instead of key,
        // to handle self-intersection
        auto prev = find_last(it, (*it).key, ic);
        if(prev == end())
            break;
        erase(it);
        it = prev;
    }
    erase(it);
    return n;
}

inline
auto
params_ref::
replace(
    iterator pos,
    param_view const& p) ->
        iterator
{
    return iterator(
        u_->edit_params(
            pos.it_,
            std::next(pos).it_,
            detail::single_param_iter(p, opt_.space_as_plus)),
        opt_);
}

inline
auto
params_ref::
replace(
    iterator from,
    iterator to,
    std::initializer_list<
        param_view> init) ->
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
params_ref::
unset(
    iterator pos) noexcept ->
        iterator
{
    BOOST_ASSERT(pos.it_.nk > 0);
    core::string_view s;
    return iterator(
        u_->edit_params(
            pos.it_,
            pos.it_.next(),
            detail::param_value_iter(
                pos.it_.nk - 1, s, false)),
        opt_);
}

inline
auto
params_ref::
set(
    iterator pos,
    core::string_view value) ->
        iterator
{
    BOOST_ASSERT(pos.it_.nk > 0);
    return iterator(
        u_->edit_params(
            pos.it_,
            pos.it_.next(),
            detail::param_value_iter(
                pos.it_.nk - 1, value, true)),
        opt_);
}

inline
auto
params_ref::
set(
    core::string_view key,
    core::string_view value,
    ignore_case_param ic) ->
        iterator
{
    // VFALCO we can't cache end() here
    // because it is invalidated
    // every time we set or erase.
    auto it0 = find(key, ic);
    if(it0 == end())
        return append({key, value});
    it0 = set(it0, value);
    auto it = end();
    for(;;)
    {
        it = find_last(it, key, ic);
        if(it == it0)
            return it0;
        it = erase(it);
    }
}

inline
auto
params_ref::
erase(
    iterator pos) noexcept ->
    iterator
{
    return erase(
        pos,
        std::next(pos));
}

inline
auto
params_ref::
erase(
    iterator first,
    iterator last) noexcept ->
        iterator
{
    core::string_view s("", 0);
    return iterator(
        u_->edit_params(
            first.it_,
            last.it_,
            detail::query_string_iter(s)),
        opt_);
}

} // urls
} // boost

#endif
