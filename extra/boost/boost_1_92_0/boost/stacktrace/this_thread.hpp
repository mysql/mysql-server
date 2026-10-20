// Copyright Antony Polukhin, 2023-2026.
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_STACKTRACE_THIS_THREAD_HPP
#define BOOST_STACKTRACE_THIS_THREAD_HPP

#include <boost/stacktrace/detail/backend_config.hpp>

#if !defined(BOOST_USE_MODULES) || defined(BOOST_STACKTRACE_INTERFACE_UNIT)

#ifdef BOOST_HAS_PRAGMA_ONCE
#   pragma once
#endif

#include <boost/stacktrace/stacktrace.hpp>

BOOST_STACKTRACE_BEGIN_MODULE_EXPORT

namespace boost { namespace stacktrace { namespace this_thread {

/// @brief Invoking the function with the enable parameter equal to `true`
/// enables capturing of stacktraces by the current thread of execution at
/// exception object construction if the `boost_stacktrace_from_exception`
/// library is linked to the current binary; disables otherwise.
///
/// Implements https://wg21.link/p2370r1
inline void set_capture_stacktraces_at_throw(bool enable = true) noexcept {
#if defined(__GNUC__) && defined(__ELF__)
    if (impl::ref_capture_stacktraces_at_throw) {
        impl::ref_capture_stacktraces_at_throw() = enable;
    }
#elif defined(BOOST_MSVC)
    if (bool* p = boost_stacktrace_impl_ref_capture_stacktraces_at_throw()) {
        *p = enable;
    }
#endif
    (void)enable;
}

/// @return whether the capturing of stacktraces by the current thread of
/// execution is enabled and
/// boost::stacktrace::basic_stacktrace::from_current_exception may return a
/// non empty stacktrace.
///
/// Returns true if set_capture_stacktraces_at_throw(false) was not called
/// and the `boost_stacktrace_from_exception` is linked to the current binary.
///
/// Implements https://wg21.link/p2370r1
inline bool get_capture_stacktraces_at_throw() noexcept {
#if defined(__GNUC__) && defined(__ELF__)
    if (impl::ref_capture_stacktraces_at_throw) {
        return impl::ref_capture_stacktraces_at_throw();
    }
#elif defined(BOOST_MSVC)
    if (bool* p = boost_stacktrace_impl_ref_capture_stacktraces_at_throw()) {
        return *p;
    }
#endif
    return false;
}

}}} // namespace boost::stacktrace::this_thread

BOOST_STACKTRACE_END_MODULE_EXPORT

#endif // !defined(BOOST_USE_MODULES) || defined(BOOST_STACKTRACE_INTERFACE_UNIT)

#endif // BOOST_STACKTRACE_THIS_THREAD_HPP
