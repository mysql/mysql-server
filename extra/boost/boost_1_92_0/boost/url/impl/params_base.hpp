//
// Copyright (c) 2019 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2022 Alan de Freitas (alandefreitas@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/url
//

#ifndef BOOST_URL_IMPL_PARAMS_BASE_HPP
#define BOOST_URL_IMPL_PARAMS_BASE_HPP

#include <boost/url/detail/params_iter_impl.hpp>
#include <boost/url/decode_view.hpp>
#include <boost/url/grammar/ci_string.hpp>
#include <iterator>
#include <ostream>
#include <string>

namespace boost {
namespace urls {

//------------------------------------------------

class BOOST_SYMBOL_VISIBLE params_base::iterator
{
    detail::params_iter_impl it_;
    bool space_as_plus_ = true;

    friend class params_base;
    friend class params_ref;

    iterator(
        detail::query_ref const& ref,
        encoding_opts opt) noexcept;

    iterator(
        detail::query_ref const& impl,
        encoding_opts opt,
        int) noexcept;

    iterator(
        detail::params_iter_impl const& it,
        encoding_opts opt) noexcept
        : it_(it)
        , space_as_plus_(opt.space_as_plus)
    {
    }

public:
    using value_type = params_base::value_type;
    using reference = params_base::reference;
    using pointer = reference;
    using difference_type =
        params_base::difference_type;
    using iterator_category =
        std::bidirectional_iterator_tag;

    iterator() = default;
    iterator(iterator const&) = default;
    iterator& operator=(
        iterator const&) noexcept = default;

    iterator&
    operator++() noexcept
    {
        it_.increment();
        return *this;
    }

    iterator
    operator++(int) noexcept
    {
        auto tmp = *this;
        ++*this;
        return tmp;
    }

    iterator&
    operator--() noexcept
    {
        it_.decrement();
        return *this;
    }

    iterator
    operator--(int) noexcept
    {
        auto tmp = *this;
        --*this;
        return tmp;
    }

    reference
    operator*() const;

    // the return value is too expensive
    pointer operator->() const = delete;

    bool
    operator==(
        iterator const& other) const noexcept
    {
        return it_.equal(other.it_);
    }

    bool
    operator!=(
        iterator const& other) const noexcept
    {
        return ! it_.equal(other.it_);
    }
};


//------------------------------------------------

inline
params_base::
iterator::
iterator(
    detail::query_ref const& ref,
    encoding_opts opt) noexcept
    : it_(ref)
    , space_as_plus_(opt.space_as_plus)
{
}

inline
params_base::
iterator::
iterator(
    detail::query_ref const& ref,
    encoding_opts opt,
    int) noexcept
    : it_(ref, 0)
    , space_as_plus_(opt.space_as_plus)
{
}


inline
auto
params_base::
iterator::
operator*() const ->
    reference

{
    encoding_opts opt;
    opt.space_as_plus =
        space_as_plus_;
    param_pct_view p =
        it_.dereference();
    return reference(
        p.key.decode(opt),
        p.value.decode(opt),
        p.has_value);
}

//------------------------------------------------
//
// params_base
//
//------------------------------------------------

inline
params_base::
params_base() noexcept
    // space_as_plus = true
    : opt_(true, false, false)
{
}

inline
bool
params_base::
contains(
    core::string_view key,
    ignore_case_param ic) const noexcept
{
    return find(
        begin(),key, ic) != end();
}

inline
auto
params_base::
find(
    core::string_view key,
    ignore_case_param ic) const noexcept ->
        iterator
{
    return iterator(
        find_impl(
            begin().it_, key, ic),
        opt_);
}

inline
auto
params_base::
find(
    iterator it,
    core::string_view key,
    ignore_case_param ic) const noexcept ->
        iterator
{
    return iterator(
        find_impl(
            it.it_, key, ic),
        opt_);
}

inline
auto
params_base::
find_last(
    core::string_view key,
    ignore_case_param ic) const noexcept ->
        iterator
{
    return iterator(
        find_last_impl(
            end().it_, key, ic),
        opt_);
}

inline
auto
params_base::
find_last(
    iterator it,
    core::string_view key,
    ignore_case_param ic) const noexcept ->
        iterator
{
    return iterator(
        find_last_impl(
            it.it_, key, ic),
        opt_);
}

inline
params_base::
params_base(
    detail::query_ref const& ref,
    encoding_opts opt) noexcept
    : ref_(ref)
    , opt_(opt)
{
}

inline
pct_string_view
params_base::
buffer() const noexcept
{
    return ref_.buffer();
}

inline
bool
params_base::
empty() const noexcept
{
    return ref_.nparam() == 0;
}

inline
std::size_t
params_base::
size() const noexcept
{
    return ref_.nparam();
}

inline
auto
params_base::
begin() const noexcept ->
    iterator
{
    return iterator(ref_, opt_);
}

inline
auto
params_base::
end() const noexcept ->
    iterator
{
    return {ref_, opt_, 0};
}

//------------------------------------------------

inline
std::size_t
params_base::
count(
    core::string_view key,
    ignore_case_param ic) const noexcept
{
    std::size_t n = 0;
    auto it = find(key, ic);
    auto const end_ = end();
    while(it != end_)
    {
        ++n;
        ++it;
        it = find(it, key, ic);
    }
    return n;
}

inline
std::string
params_base::
get_or(
    core::string_view key,
    core::string_view value,
    ignore_case_param ic) const
{
    auto it = find_impl(
        begin().it_, key, ic);
    detail::params_iter_impl end_(ref_, 0);
    if(it.equal(end_))
        return std::string(value);

    param_pct_view const p = it.dereference();
    if(! p.has_value)
        return std::string();

    auto opt = opt_;
    return p.value.decode(opt);
}

//------------------------------------------------
//
// (implementation)
//
//------------------------------------------------

inline
detail::params_iter_impl
params_base::
find_impl(
    detail::params_iter_impl it,
    core::string_view key,
    ignore_case_param ic) const noexcept
{
    detail::params_iter_impl end_(ref_, 0);
    if(! ic)
    {
        for(;;)
        {
            if(it.equal(end_))
                return it;
            if(*it.key() == key)
                return it;
            it.increment();
        }
    }
    for(;;)
    {
        if(it.equal(end_))
            return it;
        if( grammar::ci_is_equal(
                *it.key(), key))
            return it;
        it.increment();
    }
}

inline
detail::params_iter_impl
params_base::
find_last_impl(
    detail::params_iter_impl it,
    core::string_view key,
    ignore_case_param ic) const noexcept
{
    detail::params_iter_impl begin_(ref_);
    if(! ic)
    {
        for(;;)
        {
            if(it.equal(begin_))
                return { ref_, 0 };
            it.decrement();
            if(*it.key() == key)
                return it;
        }
    }
    for(;;)
    {
        if(it.equal(begin_))
            return { ref_, 0 };
        it.decrement();
        if(grammar::ci_is_equal(
                *it.key(), key))
            return it;
    }
}

//------------------------------------------------

inline
std::ostream&
operator<<(
    std::ostream& os,
    params_base const& qp)
{
    os << qp.buffer();
    return os;
}

} // urls
} // boost

#endif
