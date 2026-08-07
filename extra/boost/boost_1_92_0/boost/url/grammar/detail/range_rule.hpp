//
// Copyright (c) 2025 Alan de Freitas (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/url
//

#ifndef BOOST_URL_GRAMMAR_DETAIL_RANGE_RULE_HPP
#define BOOST_URL_GRAMMAR_DETAIL_RANGE_RULE_HPP

#include <boost/url/detail/config.hpp>
#include <type_traits>
#include <utility>

namespace boost {
namespace urls {
namespace grammar {
namespace detail {

template<class RangeRule, bool = std::is_empty<RangeRule>::value>
class range_base_storage;

template<class RangeRule>
class range_base_storage<RangeRule, false>
{
    RangeRule rule_;

protected:
    range_base_storage() = default;

    explicit
    range_base_storage(RangeRule const& rule)
        : rule_(rule)
    {
    }

    explicit
    range_base_storage(RangeRule&& rule)
        : rule_(std::move(rule))
    {
    }

    RangeRule&
    rule() noexcept
    {
        return rule_;
    }

    RangeRule const&
    rule() const noexcept
    {
        return rule_;
    }
};

template<class RangeRule>
class range_base_storage<RangeRule, true>
    : private RangeRule
{
protected:
    range_base_storage() = default;

    explicit
    range_base_storage(RangeRule const& rule)
        : RangeRule(rule)
    {
    }

    explicit
    range_base_storage(RangeRule&& rule)
        : RangeRule(std::move(rule))
    {
    }

    RangeRule&
    rule() noexcept
    {
        return *this;
    }

    RangeRule const&
    rule() const noexcept
    {
        return *this;
    }
};

} // detail
} // grammar
} // urls
} // boost

#endif
