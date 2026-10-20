// Copyright (c) 2016-2026 Antony Polukhin
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_PFR_DETAIL_FIELDS_COUNT_HPP
#define BOOST_PFR_DETAIL_FIELDS_COUNT_HPP
#pragma once

#include <boost/pfr/detail/config.hpp>
#include <boost/pfr/detail/make_integer_sequence.hpp>
#include <boost/pfr/detail/size_t_.hpp>
#include <boost/pfr/detail/unsafe_declval.hpp>

#if !defined(BOOST_PFR_INTERFACE_UNIT)
#include <limits>
#include <type_traits>
#include <utility>      // metaprogramming stuff

#if BOOST_PFR_USE_CPP26_REFLECTION
#include <meta>
#endif

#endif

#ifdef __clang__
#   pragma clang diagnostic push
#   pragma clang diagnostic ignored "-Wmissing-braces"
#   pragma clang diagnostic ignored "-Wundefined-inline"
#   pragma clang diagnostic ignored "-Wundefined-internal"
#   pragma clang diagnostic ignored "-Wmissing-field-initializers"
#endif

namespace boost { namespace pfr { namespace detail {

///////////////////// min without including <algorithm>
constexpr std::size_t min_of_size_t(std::size_t a, std::size_t b) noexcept {
    return b < a ? b : a;
}

///////////////////// Structure that can be converted to reference to anything
struct ubiq_lref_constructor {
    std::size_t ignore;
    template <class Type> constexpr operator Type&() const && noexcept {  // tweak for template_unconstrained.cpp like cases
        return detail::unsafe_declval<Type&>();
    }

    template <class Type> constexpr operator Type&() const & noexcept {  // tweak for optional_chrono.cpp like cases
        return detail::unsafe_declval<Type&>();
    }
};

///////////////////// Structure that can be converted to rvalue reference to anything
struct ubiq_rref_constructor {
    std::size_t ignore;
    template <class Type> /*constexpr*/ operator Type() const && noexcept {  // Allows initialization of rvalue reference fields and move-only types
        return detail::unsafe_declval<Type>();
    }
};

///////////////////// Hand-made is_complete<T> trait
template <typename T, typename = void>
struct is_complete : std::false_type
{};

template <typename T>
struct is_complete<T, decltype(void(sizeof(T)))> : std::integral_constant<bool, true>
{};

#ifndef __cpp_lib_is_aggregate
///////////////////// Hand-made is_aggregate_initializable_n<T> trait

// Structure that can be converted to reference to anything except reference to T
template <class T, bool IsCopyConstructible>
struct ubiq_constructor_except {
    std::size_t ignore;
    template <class Type> constexpr operator std::enable_if_t<!std::is_same<T, Type>::value, Type&> () const noexcept; // Undefined
};

template <class T>
struct ubiq_constructor_except<T, false> {
    std::size_t ignore;
    template <class Type> constexpr operator std::enable_if_t<!std::is_same<T, Type>::value, Type&&> () const noexcept; // Undefined
};


// `std::is_constructible<T, ubiq_constructor_except<T>>` consumes a lot of time, so we made a separate lazy trait for it.
template <std::size_t N, class T> struct is_single_field_and_aggregate_initializable: std::false_type {};
template <class T> struct is_single_field_and_aggregate_initializable<1, T>: std::integral_constant<
    bool, !std::is_constructible<T, ubiq_constructor_except<T, std::is_copy_constructible<T>::value>>::value
> {};

// Hand-made is_aggregate<T> trait:
// Before C++20 aggregates could be constructed from `decltype(ubiq_?ref_constructor{I})...` but type traits report that
// there's no constructor from `decltype(ubiq_?ref_constructor{I})...`
// Special case for N == 1: `std::is_constructible<T, ubiq_?ref_constructor>` returns true if N == 1 and T is copy/move constructible.
template <class T, std::size_t N, class /*Enable*/ = void>
struct is_aggregate_initializable_n {
    static constexpr bool value =
           std::is_empty<T>::value
        || std::is_array<T>::value
    ;
};

template <class T, std::size_t N>
struct is_aggregate_initializable_n<T, N, std::enable_if_t<std::is_class<T>::value && !std::is_empty<T>::value>> {
    template <std::size_t ...I>
    static constexpr bool is_not_constructible_n(std::index_sequence<I...>) noexcept {
        return (!std::is_constructible<T, decltype(ubiq_lref_constructor{I})...>::value && !std::is_constructible<T, decltype(ubiq_rref_constructor{I})...>::value)
            || is_single_field_and_aggregate_initializable<N, T>::value
        ;
    }

    static constexpr bool value = is_not_constructible_n(detail::make_index_sequence<N>{});
};

#endif // #ifndef __cpp_lib_is_aggregate

///////////////////// Detect aggregates with inheritance
template <class Derived, class U>
constexpr bool static_assert_non_inherited() noexcept {
    static_assert(
            !std::is_base_of<U, Derived>::value,
            "====================> Boost.PFR: Boost.PFR: Inherited types are not supported."
    );
    return true;
}

template <class Derived>
struct ubiq_lref_base_asserting {
    template <class Type> constexpr operator Type&() const &&  // tweak for template_unconstrained.cpp like cases
        noexcept(detail::static_assert_non_inherited<Derived, Type>())  // force the computation of assert function
    {
        return detail::unsafe_declval<Type&>();
    }

    template <class Type> constexpr operator Type&() const &  // tweak for optional_chrono.cpp like cases
        noexcept(detail::static_assert_non_inherited<Derived, Type>())  // force the computation of assert function
    {
        return detail::unsafe_declval<Type&>();
    }
};

template <class Derived>
struct ubiq_rref_base_asserting {
    template <class Type> /*constexpr*/ operator Type() const &&  // Allows initialization of rvalue reference fields and move-only types
        noexcept(detail::static_assert_non_inherited<Derived, Type>())  // force the computation of assert function
    {
        return detail::unsafe_declval<Type>();
    }
};

template <class T, std::size_t I0, std::size_t... I, class /*Enable*/ = std::enable_if_t<std::is_copy_constructible<T>::value>>
constexpr auto assert_first_not_base(std::index_sequence<I0, I...>) noexcept
    -> std::add_pointer_t<decltype(T{ ubiq_lref_base_asserting<T>{}, ubiq_lref_constructor{I}... })>
{
    return nullptr;
}

template <class T, std::size_t I0, std::size_t... I, class /*Enable*/ = std::enable_if_t<!std::is_copy_constructible<T>::value>>
constexpr auto assert_first_not_base(std::index_sequence<I0, I...>) noexcept
    -> std::add_pointer_t<decltype(T{ ubiq_rref_base_asserting<T>{}, ubiq_rref_constructor{I}... })>
{
    return nullptr;
}

template <class T>
constexpr void* assert_first_not_base(std::index_sequence<>) noexcept
{
    return nullptr;
}

template <class T, std::size_t N>
constexpr void assert_first_not_base(int) noexcept {}

template <class T, std::size_t N>
constexpr auto assert_first_not_base(long) noexcept
    -> std::enable_if_t<std::is_class<T>::value>
{
    detail::assert_first_not_base<T>(detail::make_index_sequence<N>{});
}

///////////////////// Helpers for initializable detection
// Note that these take O(N) compile time and memory!
template <class T, std::size_t... I, class /*Enable*/ = std::enable_if_t<std::is_copy_constructible<T>::value>>
constexpr auto enable_if_initializable_helper(std::index_sequence<I...>) noexcept
    -> std::add_pointer_t<decltype(T{ubiq_lref_constructor{I}...})>;

template <class T, std::size_t... I, class /*Enable*/ = std::enable_if_t<!std::is_copy_constructible<T>::value>>
constexpr auto enable_if_initializable_helper(std::index_sequence<I...>) noexcept
    -> std::add_pointer_t<decltype(T{ubiq_rref_constructor{I}...})>;

template <class T, std::size_t N, class U = std::size_t, class /*Enable*/ = decltype(detail::enable_if_initializable_helper<T>(detail::make_index_sequence<N>()))>
using enable_if_initializable_helper_t = U;

template <class T, std::size_t N>
constexpr auto is_initializable(long) noexcept
    -> detail::enable_if_initializable_helper_t<T, N, bool>
{
    return true;
}

template <class T, std::size_t N>
constexpr bool is_initializable(int) noexcept {
    return false;
}

///////////////////// Helpers for range size detection
template <std::size_t Begin, std::size_t Last>
using is_one_element_range = std::integral_constant<bool, Begin == Last>;

using multi_element_range = std::false_type;
using one_element_range = std::true_type;

#if !BOOST_PFR_USE_CPP26 && !BOOST_PFR_USE_CPP26_REFLECTION
///////////////////// Fields count next expected compiler limitation
constexpr std::size_t fields_count_compiler_limitation_next(std::size_t n) noexcept {
#if defined(_MSC_VER) && (_MSC_VER <= 1920)
    if (n < 1024)
        return 1024;
#else
    static_cast<void>(n);
#endif
    return (std::numeric_limits<std::size_t>::max)();
}

///////////////////// Fields count upper bound based on sizeof(T)
template <class T>
constexpr std::size_t fields_count_upper_bound_loose() noexcept {
    return sizeof(T) * std::numeric_limits<unsigned char>::digits + 1 /* +1 for "Arrays of Length Zero" extension */;
}

///////////////////// Fields count binary search.
// Template instantiation: depth is O(log(result)), count is O(log(result)), cost is O(result * log(result)).
template <class T, std::size_t Begin, std::size_t Last>
constexpr std::size_t fields_count_binary_search(detail::one_element_range, long) noexcept {
    static_assert(
        Begin == Last,
        "====================> Boost.PFR: Internal logic error."
    );
    return Begin;
}

template <class T, std::size_t Begin, std::size_t Last>
constexpr std::size_t fields_count_binary_search(detail::multi_element_range, int) noexcept;

template <class T, std::size_t Begin, std::size_t Last>
constexpr auto fields_count_binary_search(detail::multi_element_range, long) noexcept
    -> detail::enable_if_initializable_helper_t<T, (Begin + Last + 1) / 2>
{
    constexpr std::size_t next_v = (Begin + Last + 1) / 2;
    return detail::fields_count_binary_search<T, next_v, Last>(detail::is_one_element_range<next_v, Last>{}, 1L);
}

template <class T, std::size_t Begin, std::size_t Last>
constexpr std::size_t fields_count_binary_search(detail::multi_element_range, int) noexcept {
    constexpr std::size_t next_v = (Begin + Last + 1) / 2 - 1;
    return detail::fields_count_binary_search<T, Begin, next_v>(detail::is_one_element_range<Begin, next_v>{}, 1L);
}

template <class T, std::size_t Begin, std::size_t N>
constexpr std::size_t fields_count_upper_bound(int, int) noexcept {
    return N - 1;
}

template <class T, std::size_t Begin, std::size_t N>
constexpr auto fields_count_upper_bound(long, long) noexcept
    -> std::enable_if_t<(N > detail::fields_count_upper_bound_loose<T>()), std::size_t>
{
    static_assert(
        !detail::is_initializable<T, detail::fields_count_upper_bound_loose<T>() + 1>(1L),
        "====================> Boost.PFR: Types with user specified constructors (non-aggregate initializable types) are not supported.");
    return detail::fields_count_upper_bound_loose<T>();
}

template <class T, std::size_t Begin, std::size_t N>
constexpr auto fields_count_upper_bound(long, int) noexcept
    -> detail::enable_if_initializable_helper_t<T, N>
{
    constexpr std::size_t next_optimal = Begin + (N - Begin) * 2;
    constexpr std::size_t next = detail::min_of_size_t(next_optimal, detail::fields_count_compiler_limitation_next(N));
    return detail::fields_count_upper_bound<T, Begin, next>(1L, 1L);
}

///////////////////// Fields count lower bound linear search.
// Template instantiation: depth is O(log(result)), count is O(result), cost is O(result^2).
template <class T, std::size_t Begin, std::size_t Last, class RangeSize, std::size_t Result>
constexpr std::size_t fields_count_lower_bound(RangeSize, size_t_<Result>) noexcept {
    return Result;
}

template <class T, std::size_t Begin, std::size_t Last>
constexpr std::size_t fields_count_lower_bound(detail::one_element_range, size_t_<0> = {}) noexcept {
    static_assert(
        Begin == Last,
        "====================> Boost.PFR: Internal logic error."
    );
    return detail::is_initializable<T, Begin>(1L) ? Begin : 0;
}

template <class T, std::size_t Begin, std::size_t Last>
constexpr std::size_t fields_count_lower_bound(detail::multi_element_range, size_t_<0> = {}) noexcept {
    // Binary partition to limit template depth.
    constexpr std::size_t middle = Begin + (Last - Begin) / 2;
    constexpr std::size_t result_maybe = detail::fields_count_lower_bound<T, Begin, middle>(
        detail::is_one_element_range<Begin, middle>{}
    );
    return detail::fields_count_lower_bound<T, middle + 1, Last>(
        detail::is_one_element_range<middle + 1, Last>{},
        size_t_<result_maybe>{}
    );
}

template <class T, std::size_t Begin, std::size_t Result>
constexpr std::size_t fields_count_lower_bound_unbounded(int, size_t_<Result>) noexcept {
    return Result;
}

template <class T, std::size_t Begin>
constexpr auto fields_count_lower_bound_unbounded(long, size_t_<0>) noexcept
    -> std::enable_if_t<(Begin >= detail::fields_count_upper_bound_loose<T>()), std::size_t>
{
    static_assert(
        detail::is_initializable<T, detail::fields_count_upper_bound_loose<T>()>(1L),
        "====================> Boost.PFR: Type must be aggregate initializable.");
    return detail::fields_count_upper_bound_loose<T>();
}

template <class T, std::size_t Begin>
constexpr std::size_t fields_count_lower_bound_unbounded(int, size_t_<0>) noexcept {
    constexpr std::size_t last = detail::min_of_size_t(Begin * 2, detail::fields_count_upper_bound_loose<T>()) - 1;
    constexpr std::size_t result_maybe = detail::fields_count_lower_bound<T, Begin, last>(
        detail::is_one_element_range<Begin, last>{}
    );
    return detail::fields_count_lower_bound_unbounded<T, last + 1>(1L, size_t_<result_maybe>{});
}
#endif

///////////////////// Choosing between array size, unbounded binary search, and linear search followed by unbounded binary search.
template <class T>
constexpr auto fields_count_dispatch(long, long, std::false_type /*are_preconditions_met*/) noexcept {
    return 0;
}

template <class T>
constexpr auto fields_count_dispatch(long, long, std::true_type /*are_preconditions_met*/) noexcept
    -> std::enable_if_t<std::is_array<T>::value, std::size_t>
{
    return sizeof(T) / sizeof(std::remove_all_extents_t<T>);
}

#if BOOST_PFR_USE_CPP26_REFLECTION
template<class T>
constexpr auto fields_count_dispatch_impl(const T &) noexcept
{
    return std::integral_constant<std::size_t,
        nonstatic_data_members_of(
            ^^T,
            std::meta::access_context::current()
        ).size()
    >{};
}

template<class T>
constexpr auto fields_count_dispatch(long, int, std::true_type /*are_preconditions_met*/) noexcept
    -> std::enable_if_t<std::is_scalar<T>::value, std::size_t>
{
    return 1;
}

template<class T>
constexpr auto fields_count_dispatch(int, int, std::true_type /*are_preconditions_met*/) noexcept
{
    return decltype(detail::fields_count_dispatch_impl(std::declval<const T &>()))::value;
}
#elif BOOST_PFR_USE_CPP26
template<class T>
constexpr auto fields_count_dispatch_impl(const T &t) noexcept
{
    const auto &[... elts] = t;
    return std::integral_constant<std::size_t, sizeof...(elts)>{};
}

template<class T>
constexpr auto fields_count_dispatch(long, int, std::true_type /*are_preconditions_met*/) noexcept
    -> std::enable_if_t<std::is_scalar<T>::value, std::size_t>
{
    return 1;
}

template<class T>
constexpr auto fields_count_dispatch(int, int, std::true_type /*are_preconditions_met*/) noexcept
{
    return decltype(detail::fields_count_dispatch_impl(std::declval<const T &>()))::value;
}
#else
template <class T>
constexpr auto fields_count_dispatch(long, int, std::true_type /*are_preconditions_met*/) noexcept
    -> decltype(sizeof(T{}))
{
    constexpr std::size_t typical_fields_count = 4;
    constexpr std::size_t last = detail::fields_count_upper_bound<T, typical_fields_count / 2, typical_fields_count>(1L, 1L);
    return detail::fields_count_binary_search<T, 0, last>(detail::is_one_element_range<0, last>{}, 1L);
}

template <class T>
constexpr std::size_t fields_count_dispatch(int, int, std::true_type /*are_preconditions_met*/) noexcept {
    // T is not default aggregate initializable. This means that at least one of the members is not default-constructible.
    // Use linear search to find the smallest valid initializer, after which we unbounded binary search for the largest.
    constexpr std::size_t begin = detail::fields_count_lower_bound_unbounded<T, 1>(1L, size_t_<0>{});

    constexpr std::size_t last = detail::fields_count_upper_bound<T, begin, begin + 1>(1L, 1L);
    return detail::fields_count_binary_search<T, begin, last>(detail::is_one_element_range<begin, last>{}, 1L);
}
#endif

///////////////////// Returns fields count
template <class T>
constexpr std::size_t fields_count() noexcept {
    using type = std::remove_cv_t<T>;

    constexpr bool type_is_complete = detail::is_complete<type>::value;
    static_assert(
        type_is_complete,
        "====================> Boost.PFR: Type must be complete."
    );

    constexpr bool type_is_not_a_reference = !std::is_reference<type>::value
         || !type_is_complete // do not show assert if previous check failed
    ;
    static_assert(
        type_is_not_a_reference,
        "====================> Boost.PFR: Attempt to get fields count on a reference. This is not allowed because that could hide an issue and different library users expect different behavior in that case."
    );

#if BOOST_PFR_HAS_GUARANTEED_COPY_ELISION
    constexpr bool type_fields_are_move_constructible = true;
#else
    constexpr bool type_fields_are_move_constructible =
        std::is_copy_constructible<std::remove_all_extents_t<type>>::value || (
            std::is_move_constructible<std::remove_all_extents_t<type>>::value
            && std::is_move_assignable<std::remove_all_extents_t<type>>::value
        )
        || !type_is_not_a_reference // do not show assert if previous check failed
    ;
    static_assert(
        type_fields_are_move_constructible,
        "====================> Boost.PFR: Type and each field in the type must be copy constructible (or move constructible and move assignable)."
    );
#endif  // #if !BOOST_PFR_HAS_GUARANTEED_COPY_ELISION

    constexpr bool type_is_not_polymorphic = !std::is_polymorphic<type>::value;
    static_assert(
        type_is_not_polymorphic,
        "====================> Boost.PFR: Type must have no virtual function, because otherwise it is not aggregate initializable."
    );

#ifdef __cpp_lib_is_aggregate
    constexpr bool type_is_aggregate =
        std::is_aggregate<type>::value             // Does not return `true` for built-in types.
        || std::is_scalar<type>::value;
    static_assert(
        type_is_aggregate,
        "====================> Boost.PFR: Type must be aggregate initializable."
    );
#else
    constexpr bool type_is_aggregate = true;
#endif

// Can't use the following. See the non_std_layout.cpp test.
//#if !BOOST_PFR_USE_CPP17
//    static_assert(
//        std::is_standard_layout<type>::value,   // Does not return `true` for structs that have non standard layout members.
//        "Type must be aggregate initializable."
//    );
//#endif

    constexpr bool no_errors =
        type_is_complete && type_is_not_a_reference && type_fields_are_move_constructible
        && type_is_not_polymorphic && type_is_aggregate;
    constexpr std::size_t result
        = detail::fields_count_dispatch<type>(1L, 1L, std::integral_constant<bool, no_errors>{});
    detail::assert_first_not_base<type, result>(1L);

#ifndef __cpp_lib_is_aggregate
    constexpr bool type_is_aggregate_initializable_n =
        detail::is_aggregate_initializable_n<type, result>::value  // Does not return `true` for built-in types.
        || std::is_scalar<type>::value;
    static_assert(
        type_is_aggregate_initializable_n,
        "====================> Boost.PFR: Types with user specified constructors (non-aggregate initializable types) are not supported."
    );
#else
    constexpr bool type_is_aggregate_initializable_n = true;
#endif

    static_assert(
        result != 0 || std::is_empty<type>::value || std::is_fundamental<type>::value || std::is_reference<type>::value || !no_errors || !type_is_aggregate_initializable_n,
        "====================> Boost.PFR: If there's no other failed static asserts then something went wrong. Please report this issue to the github along with the structure you're reflecting."
    );

    return result;
}

}}} // namespace boost::pfr::detail

#ifdef __clang__
#   pragma clang diagnostic pop
#endif

#endif // BOOST_PFR_DETAIL_FIELDS_COUNT_HPP
