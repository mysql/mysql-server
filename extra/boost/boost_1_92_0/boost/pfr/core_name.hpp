// Copyright (c) 2023 Bela Schaum, X-Ryl669, Denis Mikhailov.
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)


// Initial implementation by Bela Schaum, https://github.com/schaumb
// The way to make it union and UB free by X-Ryl669, https://github.com/X-Ryl669
//

#ifndef BOOST_PFR_CORE_NAME_HPP
#define BOOST_PFR_CORE_NAME_HPP
#pragma once

#include <boost/pfr/detail/config.hpp>

#if !defined(BOOST_USE_MODULES) || defined(BOOST_PFR_INTERFACE_UNIT)

#include <boost/pfr/detail/core_name.hpp>

#include <boost/pfr/detail/sequence_tuple.hpp>
#include <boost/pfr/detail/stdarray.hpp>
#include <boost/pfr/detail/make_integer_sequence.hpp>

#include <boost/pfr/tuple_size.hpp>

#if !defined(BOOST_PFR_INTERFACE_UNIT)
#include <cstddef> // for std::size_t

#if BOOST_PFR_CORE_NAME_ENABLED
#include <string_view>
#endif
#endif

/// \file boost/pfr/core_name.hpp
/// Contains functions \forcedlink{get_name} and \forcedlink{names_as_array} to know which names each field of any \aggregate has.
///
/// \fnrefl for details.
///
/// \b Synopsis:

namespace boost { namespace pfr {

BOOST_PFR_BEGIN_MODULE_EXPORT

/// \brief Returns name of a field with index `I` in \aggregate `T`.
///
/// \b Example:
/// \code
///     struct my_struct { int i, short s; };
///
///     assert(boost::pfr::get_name<0, my_struct>() == "i");
///     assert(boost::pfr::get_name<1, my_struct>() == "s");
/// \endcode
template <std::size_t I, class T>
constexpr
#ifdef BOOST_PFR_DOXYGEN_INVOKED
std::string_view
#else
auto
#endif
get_name() noexcept {
    return detail::get_name<T, I>();
}

// FIXME: implement this
// template<class U, class T>
// constexpr auto get_name() noexcept {
//     return detail::sequence_tuple::get_by_type_impl<U>( detail::tie_as_names_tuple<T>() );
// }

/// \brief Creates a `std::array` from names of fields of an \aggregate `T`.
///
/// \b Example:
/// \code
///     struct my_struct { int i, short s; };
///     std::array<std::string_view, 2> a = boost::pfr::names_as_array<my_struct>();
///     assert(a[0] == "i");
/// \endcode
template <class T>
constexpr
#ifdef BOOST_PFR_DOXYGEN_INVOKED
std::array<std::string_view, boost::pfr::tuple_size_v<T>>
#else
auto
#endif
names_as_array() noexcept {
    return detail::make_stdarray_from_tietuple(
        detail::tie_as_names_tuple<T>(),
        detail::make_index_sequence< tuple_size_v<T> >()
    );
}


/// Calls `func` for each field with its name of a `value`
///
/// \param func must have one of the following signatures:
///     * any_return_type func(std::string_view name, U&& field)                // field of value is perfect forwarded to function
///     * any_return_type func(std::string_view name, U&& field, std::size_t i)
///     * any_return_type func(std::string_view name, U&& value, I i)           // Here I is an `std::integral_constant<size_t, field_index>`
///
/// \param value To each field of this variable will be the `func` applied.
///
/// \b Example:
/// \code
///     struct Toto { int a; char c; };
///     Toto t {5, 'c'};
///     auto print = [](std::string_view name, const auto& value){ std::cout << "Name: " << name << " Value: " << value << std::endl; };
///     for_each_field_with_name(t, print);
/// \endcode
template <class T, class F>
constexpr void for_each_field_with_name(T&& value, F&& func) {
#if BOOST_PFR_CORE_NAME_ENABLED
    return boost::pfr::detail::for_each_field(
        std::forward<T>(value),
        [&func](auto&& field, auto index) {
            using IndexType = decltype(index);
            using FieldType = decltype(field);
            constexpr auto name = boost::pfr::detail::get_name<std::remove_reference_t<T>, IndexType::value>();
            if constexpr (std::is_invocable_v<F, std::string_view, FieldType, IndexType>) {
                std::forward<F>(func)(name, std::forward<FieldType>(field), index);
            } else {
                std::forward<F>(func)(name, std::forward<FieldType>(field));
            }
        });
#else
    boost::pfr::detail::report_name_reflection_mising_requirement<T>();
    (void)value;
    (void)func;
#endif
}

BOOST_PFR_END_MODULE_EXPORT

}} // namespace boost::pfr

#endif  // #if !defined(BOOST_USE_MODULES) || defined(BOOST_PFR_INTERFACE_UNIT)

#endif // BOOST_PFR_CORE_NAME_HPP
