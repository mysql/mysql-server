//
// Copyright (c) 2019 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2022 Alan de Freitas (alandefreitas@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/url
//

#ifndef BOOST_URL_IMPL_PARAMS_ENCODED_BASE_HPP
#define BOOST_URL_IMPL_PARAMS_ENCODED_BASE_HPP

#include <boost/url/detail/params_iter_impl.hpp>
#include <boost/url/grammar/ci_string.hpp>
#include <ostream>

namespace boost {
namespace urls {

#ifndef BOOST_URL_DOCS
class params_ref;
#endif

//------------------------------------------------

class params_encoded_base::iterator
{
    detail::params_iter_impl it_;

    friend class params_encoded_base;
    friend class params_encoded_ref;

    iterator(detail::query_ref const& ref) noexcept;
    iterator(detail::query_ref const& ref, int) noexcept;
    iterator(
        detail::params_iter_impl const& it)
        : it_(it)
    {
    }

public:
    using value_type =
        params_encoded_base::value_type;
    using reference =
        params_encoded_base::reference;
    using pointer = reference;
    using difference_type = std::ptrdiff_t;
    using iterator_category =
        std::bidirectional_iterator_tag;

    iterator() = default;
    iterator(iterator const&) = default;
    iterator& operator=(
        iterator const&) = default;

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
    operator*() const
    {
        return it_.dereference();
    }

    pointer
    operator->() const
    {
        return it_.dereference();
    }

    friend
    bool
    operator==(
        iterator const& it0,
        iterator const& it1) noexcept
    {
        return it0.it_.equal(it1.it_);
    }

    friend
    bool
    operator!=(
        iterator const& it0,
        iterator const& it1) noexcept
    {
        return ! it0.it_.equal(it1.it_);
    }
};

//------------------------------------------------
//
// Observers
//
//------------------------------------------------

inline
bool
params_encoded_base::
contains(
    pct_string_view key,
    ignore_case_param ic) const noexcept
{
    return find_impl(
        begin().it_, key, ic) != end();
}

inline
pct_string_view
params_encoded_base::
get_or(
    pct_string_view key,
    pct_string_view value,
    ignore_case_param ic) const noexcept
{
    auto it = find_impl(
        begin().it_, key, ic);
    detail::params_iter_impl end_(ref_, 0);
    if(it.equal(end_))
        return value;

    param_pct_view const p = it.dereference();
    if(! p.has_value)
        return pct_string_view();

    return p.value;
}

inline
auto
params_encoded_base::
find(
    pct_string_view key,
    ignore_case_param ic) const noexcept ->
        iterator
{
    return find_impl(
        begin().it_, key, ic);
}

inline
auto
params_encoded_base::
find(
    iterator it,
    pct_string_view key,
    ignore_case_param ic) const noexcept ->
        iterator
{
    return find_impl(
        it.it_, key, ic);
}

inline
auto
params_encoded_base::
find_last(
    pct_string_view key,
    ignore_case_param ic) const noexcept ->
        iterator
{
    return find_last_impl(
        end().it_, key, ic);
}

inline
auto
params_encoded_base::
find_last(
    iterator it,
    pct_string_view key,
    ignore_case_param ic) const noexcept ->
        iterator
{
    return find_last_impl(
        it.it_, key, ic);
}

//------------------------------------------------

inline
params_encoded_base::
iterator::
iterator(
    detail::query_ref const& ref) noexcept
    : it_(ref)
{
}

inline
params_encoded_base::
iterator::
iterator(
    detail::query_ref const& ref, int) noexcept
    : it_(ref, 0)
{
}

//------------------------------------------------
//
// params_encoded_base
//
//------------------------------------------------

inline
params_encoded_base::
params_encoded_base(
    detail::query_ref const& ref) noexcept
    : ref_(ref)
{
}

//------------------------------------------------
//
// Observers
//
//------------------------------------------------

inline
pct_string_view
params_encoded_base::
buffer() const noexcept
{
    return ref_.buffer();
}

inline
bool
params_encoded_base::
empty() const noexcept
{
    return ref_.nparam() == 0;
}

inline
std::size_t
params_encoded_base::
size() const noexcept
{
    return ref_.nparam();
}

inline
auto
params_encoded_base::
begin() const noexcept ->
    iterator
{
    return { ref_ };
}

inline
auto
params_encoded_base::
end() const noexcept ->
    iterator
{
    return { ref_, 0 };
}

//------------------------------------------------

inline
std::size_t
params_encoded_base::
count(
    pct_string_view key,
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

//------------------------------------------------
//
// (implementation)
//
//------------------------------------------------

inline
detail::params_iter_impl
params_encoded_base::
find_impl(
    detail::params_iter_impl it,
    pct_string_view key,
    ignore_case_param ic) const noexcept
{
    detail::params_iter_impl end_(ref_, 0);
    if(! ic)
    {
        for(;;)
        {
            if(it.equal(end_))
                return it;
            if(*it.key() == *key)
                return it;
            it.increment();
        }
    }
    for(;;)
    {
        if(it.equal(end_))
            return it;
        if( grammar::ci_is_equal(
                *it.key(), *key))
            return it;
        it.increment();
    }
}

inline
detail::params_iter_impl
params_encoded_base::
find_last_impl(
    detail::params_iter_impl it,
    pct_string_view key,
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
            if(*it.key() == *key)
                return it;
        }
    }
    for(;;)
    {
        if(it.equal(begin_))
            return { ref_, 0 };
        it.decrement();
        if(grammar::ci_is_equal(
                *it.key(), *key))
            return it;
    }
}

//------------------------------------------------

inline
std::ostream&
operator<<(
    std::ostream& os,
    params_encoded_base const& qp)
{
    os << qp.buffer();
    return os;
}

} // urls
} // boost

#endif
