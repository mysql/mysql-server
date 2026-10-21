// Copyright (c) 2016-2026 Antony Polukhin
// Copyright (c) 2025 Jean-Michaël Celerier
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

// boost-no-inspect

#ifndef BOOST_PFR_DETAIL_CORE26_HPP
#define BOOST_PFR_DETAIL_CORE26_HPP
#pragma once

#include <boost/pfr/detail/sequence_tuple.hpp>

namespace boost::pfr::detail {

template<class T>
constexpr auto tie_as_tuple(T &val) noexcept {
    static_assert(!std::is_union<T>::value,
                  "====================> Boost.PFR: For safety reasons it is forbidden to reflect "
                  "unions. See `Reflection of unions` section in the docs for more info.");
    auto &&[... members] = std::forward<T>(val);
    return sequence_tuple::tuple<std::add_lvalue_reference_t<decltype(members)>...>{members...};
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
