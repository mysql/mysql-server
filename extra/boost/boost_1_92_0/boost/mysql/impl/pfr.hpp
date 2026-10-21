//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_PFR_HPP
#define BOOST_MYSQL_IMPL_PFR_HPP

#pragma once

#include <boost/config.hpp>

// Silence MSVC 14.1 warnings caused by https://github.com/boostorg/pfr/issues/167
#if defined(BOOST_MSVC) && BOOST_MSVC < 1920
#pragma warning(push)
#pragma warning(disable : 4100)
#endif

#include <boost/mysql/pfr.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/mysql/detail/typing/row_traits.hpp>

#include <boost/pfr/core.hpp>
#include <boost/pfr/core_name.hpp>
#include <boost/pfr/traits.hpp>

#include <type_traits>
#include <utility>

#if BOOST_PFR_CORE_NAME_ENABLED
#include <array>
#include <cstddef>
#include <string_view>
#endif

namespace boost {
namespace mysql {
namespace detail {

// Not all types reflected by PFR are acceptable for us - this function performs this checking
template <class T>
constexpr bool is_pfr_reflectable() noexcept
{
    return std::is_class<T>::value && !std::is_const<T>::value
    // is_implicitly_reflectable_v returns always false when implicit reflection
    // (which requires structured bindings and C++17) is not available
#if BOOST_PFR_ENABLE_IMPLICIT_REFLECTION
           && pfr::is_implicitly_reflectable_v<T, struct mysql_tag>
#endif
        ;
}

template <class T>
using pfr_fields_t = decltype(pfr::structure_to_tuple(std::declval<const T&>()));

#if BOOST_PFR_CORE_NAME_ENABLED

// PFR field names use std::string_view
template <std::size_t N>
constexpr std::array<string_view, N> to_name_table_storage(std::array<std::string_view, N> input) noexcept
{
    std::array<string_view, N> res{};
    for (std::size_t i = 0; i < N; ++i)
        res[i] = input[i];
    return res;
}

template <class T>
constexpr inline std::size_t pfr_row_size_v = mp11::mp_size<pfr_fields_t<T>>::value;

template <class T>
constexpr std::array<string_view, pfr_row_size_v<T>> create_pfr_name_table() noexcept
{
    // Some MSVC compilers have trouble with pfr::names_as_array<T>() when
    // the row type is empty
    if constexpr (pfr_row_size_v<T> == 0u)
    {
        return {};
    }
    else
    {
        return to_name_table_storage(pfr::names_as_array<T>());
    }
}

template <class T>
constexpr inline auto pfr_names_storage = create_pfr_name_table<T>();

template <class T>
class row_traits<pfr_by_name<T>, false>
{
    static_assert(
        is_pfr_reflectable<T>(),
        "T needs to be a non-const object type that supports PFR reflection"
    );

public:
    using underlying_row_type = T;
    using field_types = pfr_fields_t<T>;

    static constexpr name_table_t name_table() noexcept { return pfr_names_storage<T>; }

    template <class F>
    static void for_each_member(T& to, F&& function)
    {
        pfr::for_each_field(to, std::forward<F>(function));
    }
};

#endif

template <class T>
class row_traits<pfr_by_position<T>, false>
{
    static_assert(
        is_pfr_reflectable<T>(),
        "T needs to be a non-const object type that supports PFR reflection"
    );

public:
    using underlying_row_type = T;
    using field_types = pfr_fields_t<T>;

    static constexpr name_table_t name_table() noexcept { return {}; }

    template <class F>
    static void for_each_member(T& to, F&& function)
    {
        pfr::for_each_field(to, std::forward<F>(function));
    }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#if defined(BOOST_MSVC) && BOOST_MSVC < 1920
#pragma warning(pop)
#endif

#endif
