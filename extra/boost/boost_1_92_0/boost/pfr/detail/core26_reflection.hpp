// Copyright (c) 2025-2026 Antony Polukhin
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_PFR_DETAIL_CORE26_REFLECTION_HPP
#define BOOST_PFR_DETAIL_CORE26_REFLECTION_HPP
#pragma once

#include <boost/pfr/detail/config.hpp>
#include <boost/pfr/detail/fields_count.hpp>
#include <boost/pfr/detail/make_integer_sequence.hpp>
#include <boost/pfr/detail/sequence_tuple.hpp>

#if !defined(BOOST_PFR_INTERFACE_UNIT)
#include <type_traits>
#include <meta>
#endif

namespace boost::pfr::detail {

template <std::size_t I, typename T>
consteval decltype(auto) reference_by_index(T &val) noexcept {
    return val.[:
        nonstatic_data_members_of(
            ^^T,
            std::meta::access_context::current()
        ).at(I)
    :];
}

template<class T, std::size_t... I>
constexpr auto tie_as_tuple_impl(T &val, std::index_sequence<I...>) noexcept {
    return sequence_tuple::tuple<
        decltype(boost::pfr::detail::reference_by_index<I>(val))...
    >{
        boost::pfr::detail::reference_by_index<I>(val)...
    };
    
}

template<class T>
constexpr auto tie_as_tuple(T &val) noexcept {
    return detail::tie_as_tuple_impl(
        val,
        detail::make_index_sequence<detail::fields_count<T>()>{}
    );
    
}

template <class T, class F, std::size_t... I>
constexpr void for_each_field_dispatcher(T& t, F&& f, std::index_sequence<I...>) {
    static_assert(
        !std::is_union<T>::value,
        "====================> Boost.PFR: For safety reasons it is forbidden to reflect unions. See `Reflection of unions` section in the docs for more info."
    );
    std::forward<F>(f)(
        detail::tie_as_tuple(t)
    );
}

} // namespace boost::pfr::detail

#endif
