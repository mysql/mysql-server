// Copyright (c) 2019-2026 Antony Polukhin.
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_PFR_DETAIL_UNSAFE_DECLVAL_HPP
#define BOOST_PFR_DETAIL_UNSAFE_DECLVAL_HPP
#pragma once

#include <boost/pfr/detail/config.hpp>

#if !defined(BOOST_PFR_INTERFACE_UNIT)
#include <type_traits>
#endif

namespace boost { namespace pfr { namespace detail {

// This function serves as a link-time assert. If linker requires it, then
// `unsafe_declval()` is used at runtime.
void report_if_you_see_link_error_with_this_function() noexcept;

// For returning non default constructible types. Do NOT use at runtime!
//
// GCCs std::declval may not be used in potentionally evaluated contexts,
// so we reinvent it.
template <class T>
constexpr T unsafe_declval() noexcept {
    report_if_you_see_link_error_with_this_function();

#ifdef BOOST_PFR_USE_LEGACY_UNSAFE_DECLVAL_IMPLEMENTATION
    typename std::remove_reference<T>::type* ptr = nullptr;
    ptr += 42; // suppresses 'null pointer dereference' warnings
    return static_cast<T>(*ptr);
#else
    // Looks like `static_cast<T>(*ptr)` to prvalue fails on clang in C++26.
    // If this new implementation does not work for some cases, please, fill a
    // bug report and feel free to
    // define BOOST_PFR_USE_LEGACY_UNSAFE_DECLVAL_IMPLEMENTATION.
    using func_ptr_t = T(*)();
    func_ptr_t ptr = nullptr;
    return ptr();
#endif
}

}}} // namespace boost::pfr::detail


#endif // BOOST_PFR_DETAIL_UNSAFE_DECLVAL_HPP

