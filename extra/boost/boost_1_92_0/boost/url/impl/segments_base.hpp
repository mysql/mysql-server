//
// Copyright (c) 2019 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2022 Alan de Freitas (alandefreitas@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/url
//

#ifndef BOOST_URL_IMPL_SEGMENTS_BASE_HPP
#define BOOST_URL_IMPL_SEGMENTS_BASE_HPP

#include <boost/url/detail/segments_iter_impl.hpp>
#include <boost/url/encoding_opts.hpp>
#include <boost/assert.hpp>
#include <iterator>
#include <ostream>

namespace boost {
namespace urls {
namespace detail {
struct segments_iter_access;
}

class segments_base::iterator
{
    detail::segments_iter_impl it_;

    friend class segments_base;
    friend class segments_ref;
    friend struct detail::segments_iter_access;

    iterator(detail::path_ref const&) noexcept;
    iterator(detail::path_ref const&, int) noexcept;

    iterator(
        detail::segments_iter_impl const& it) noexcept
        : it_(it)
    {
    }

public:
    using value_type = segments_base::value_type;
    using reference = segments_base::reference;
    using pointer = reference;
    using difference_type =
        segments_base::difference_type;
    using iterator_category =
        std::bidirectional_iterator_tag;

    iterator() = default;
    iterator(iterator const&) = default;
    iterator& operator=(
        iterator const&) noexcept = default;

    reference
    operator*() const
    {
        encoding_opts opt;
        opt.space_as_plus = false;
        return it_.dereference().decode(opt);
    }

    // the return value is too expensive
    pointer operator->() const = delete;

    iterator&
    operator++() noexcept
    {
        it_.increment();
        return *this;
    }

    iterator&
    operator--() noexcept
    {
        it_.decrement();
        return *this;
    }

    iterator
    operator++(int) noexcept
    {
        auto tmp = *this;
        ++*this;
        return tmp;
    }

    iterator
    operator--(int) noexcept
    {
        auto tmp = *this;
        --*this;
        return tmp;
    }

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
segments_base::
iterator::
iterator(
    detail::path_ref const& ref) noexcept
    : it_(ref)
{
}

inline
segments_base::
iterator::
iterator(
    detail::path_ref const& ref,
    int) noexcept
    : it_(ref, 0)
{
}

//------------------------------------------------
//
// segments_base
//
//------------------------------------------------

inline
segments_base::
segments_base(
    detail::path_ref const& ref) noexcept
    : ref_(ref)
{
}

inline
pct_string_view
segments_base::
buffer() const noexcept
{
    return ref_.buffer();
}

inline
bool
segments_base::
is_absolute() const noexcept
{
    return ref_.buffer().starts_with('/');
}

inline
bool
segments_base::
empty() const noexcept
{
    return ref_.nseg() == 0;
}

inline
std::size_t
segments_base::
size() const noexcept
{
    return ref_.nseg();
}

inline
std::string
segments_base::
front() const
{
    BOOST_ASSERT(! empty());
    return *begin();
}

inline
std::string
segments_base::
back() const
{
    BOOST_ASSERT(! empty());
    return *--end();
}

inline
auto
segments_base::
begin() const noexcept ->
    iterator
{
    return iterator(ref_);
}

inline
auto
segments_base::
end() const noexcept ->
    iterator
{
    return iterator(ref_, 0);
}

//------------------------------------------------

inline
std::ostream&
operator<<(
    std::ostream& os,
    segments_base const& ps)
{
    os << ps.buffer();
    return os;
}

} // urls
} // boost

#endif
