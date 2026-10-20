// Copyright (c) 2016-2026 Antony Polukhin
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_PFR_DETAIL_FOR_EACH_FIELD_HPP
#define BOOST_PFR_DETAIL_FOR_EACH_FIELD_HPP
#pragma once

#include <boost/pfr/detail/config.hpp>

#include <boost/pfr/detail/core.hpp>
#include <boost/pfr/detail/fields_count.hpp>
#include <boost/pfr/detail/for_each_field_impl.hpp>
#include <boost/pfr/detail/make_integer_sequence.hpp>

#if !defined(BOOST_PFR_INTERFACE_UNIT)
#include <type_traits>      // metaprogramming stuff
#include <utility>          // forward_like
#endif

namespace boost { namespace pfr { namespace detail {

template <class T, class F>
constexpr void for_each_field(T&& value, F&& func) {
#if BOOST_PFR_USE_CPP26
    using no_ref = std::remove_reference_t<T>;
    if constexpr (std::is_aggregate_v<no_ref> || std::is_bounded_array_v<no_ref>) {
        auto &&[... members] = value;
        ::boost::pfr::detail::for_each_field_impl(value,
                                                  std::forward<F>(func),
                                                  std::make_index_sequence<sizeof...(members)>{},
                                                  std::is_rvalue_reference<T &&>{});
    } else {
        ::boost::pfr::detail::for_each_field_impl(value,
                                                  std::forward<F>(func),
                                                  std::make_index_sequence<1>{},
                                                  std::is_rvalue_reference<T &&>{});
    }
#else
    constexpr std::size_t fields_count_val = boost::pfr::detail::fields_count<std::remove_reference_t<T>>();

    ::boost::pfr::detail::for_each_field_dispatcher(
        value,
        [&func](auto&& t) mutable {
            // MSVC related workaround. Its lambdas do not capture constexprs.
            constexpr std::size_t fields_count_val_in_lambda
                = boost::pfr::detail::fields_count<std::remove_reference_t<T>>();

            ::boost::pfr::detail::for_each_field_impl(
                t,
                std::forward<F>(func),
                detail::make_index_sequence<fields_count_val_in_lambda>{},
                std::is_rvalue_reference<T&&>{}
            );
        },
        detail::make_index_sequence<fields_count_val>{}
    );
#endif
}

}}} // namespace boost::pfr::detail


#endif // BOOST_PFR_DETAIL_FOR_EACH_FIELD_HPP
