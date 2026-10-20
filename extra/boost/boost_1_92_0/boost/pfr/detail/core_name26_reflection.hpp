// Copyright (c) 2025-2026 Antony Polukhin
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)


// Initial implementation by Bela Schaum, https://github.com/schaumb
// The way to make it union and UB free by X-Ryl669, https://github.com/X-Ryl669
//

#ifndef BOOST_PFR_DETAIL_CORE_NAME26_REFLECTION_HPP
#define BOOST_PFR_DETAIL_CORE_NAME26_REFLECTION_HPP
#pragma once

#include <boost/pfr/detail/config.hpp>

#include <boost/pfr/detail/fields_count.hpp>
#include <boost/pfr/detail/for_each_field.hpp>
#include <boost/pfr/detail/make_integer_sequence.hpp>
#include <boost/pfr/detail/sequence_tuple.hpp>

#if !defined(BOOST_PFR_INTERFACE_UNIT)
#include <type_traits>
#include <array>
#include <memory> // for std::addressof
#include <meta>
#endif

namespace boost { namespace pfr { namespace detail {

template <class T, std::size_t I>
constexpr std::string_view get_name() noexcept {
    static_assert(
        !std::is_union<T>::value,
        "====================> Boost.PFR: For safety reasons it is forbidden to reflect unions. See `Reflection of unions` section in the docs for more info."
    );
    static_assert(
        !std::is_array<T>::value,
        "====================> Boost.PFR: It is impossible to extract name from old C array since it doesn't have named members"
    );

    return identifier_of(
        nonstatic_data_members_of(
            ^^T,
            std::meta::access_context::current()
        ).at(I)
    );
}

template <class T, std::size_t... I>
constexpr auto tie_as_names_tuple_impl(std::index_sequence<I...>) noexcept {
    return detail::sequence_tuple::make_sequence_tuple(detail::get_name<T, I>()...);
}

template <class T>
constexpr auto tie_as_names_tuple() noexcept {
    return detail::tie_as_names_tuple_impl<T>(detail::make_index_sequence<detail::fields_count<T>()>{});
}

}}} // namespace boost::pfr::detail

#endif // BOOST_PFR_DETAIL_CORE_NAME20_STATIC_HPP

