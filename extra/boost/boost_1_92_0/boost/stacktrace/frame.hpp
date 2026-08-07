// Copyright Antony Polukhin, 2016-2026.
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_STACKTRACE_FRAME_HPP
#define BOOST_STACKTRACE_FRAME_HPP

#include <boost/stacktrace/detail/backend_config.hpp>

#if !defined(BOOST_USE_MODULES) || defined(BOOST_STACKTRACE_INTERFACE_UNIT)

#ifdef BOOST_HAS_PRAGMA_ONCE
#   pragma once
#endif

#if !defined(BOOST_STACKTRACE_INTERFACE_UNIT) && !defined(BOOST_STACKTRACE_USE_STD_MODULE)
#include <iosfwd>
#include <string>
#endif // !defined(BOOST_STACKTRACE_INTERFACE_UNIT) && !defined(BOOST_STACKTRACE_USE_STD_MODULE)

#include <boost/stacktrace/detail/frame_decl.hpp>
#include <boost/stacktrace/detail/push_options.h>

#if defined(BOOST_MSVC) && (defined(BOOST_STACKTRACE_INTERNAL_BUILD_LIBS) || !defined(BOOST_STACKTRACE_LINK))
extern "C" {

#if defined(BOOST_STACKTRACE_DYN_LINK)
BOOST_SYMBOL_EXPORT
#elif defined(BOOST_STACKTRACE_LINK)
#else
BOOST_SYMBOL_EXPORT inline
#endif
void* boost_stacktrace_impl_return_nullptr() { return nullptr; }

}
#endif

BOOST_STACKTRACE_BEGIN_MODULE_EXPORT

namespace boost { namespace stacktrace {

/// Comparison operators that provide platform dependant ordering and have O(1) complexity; are Async-Handler-Safe.
constexpr inline bool operator< (const frame& lhs, const frame& rhs) noexcept { return lhs.address() < rhs.address(); }
constexpr inline bool operator> (const frame& lhs, const frame& rhs) noexcept { return rhs < lhs; }
constexpr inline bool operator<=(const frame& lhs, const frame& rhs) noexcept { return !(lhs > rhs); }
constexpr inline bool operator>=(const frame& lhs, const frame& rhs) noexcept { return !(lhs < rhs); }
constexpr inline bool operator==(const frame& lhs, const frame& rhs) noexcept { return lhs.address() == rhs.address(); }
constexpr inline bool operator!=(const frame& lhs, const frame& rhs) noexcept { return !(lhs == rhs); }

/// Fast hashing support, O(1) complexity; Async-Handler-Safe.
inline std::size_t hash_value(const frame& f) noexcept {
    return reinterpret_cast<std::size_t>(f.address());
}

/// Outputs stacktrace::frame in a human readable format to string; unsafe to use in async handlers.
BOOST_STACKTRACE_FUNCTION std::string to_string(const frame& f);

/// Outputs stacktrace::frame in a human readable format to output stream; unsafe to use in async handlers.
template <class CharT, class TraitsT>
std::basic_ostream<CharT, TraitsT>& operator<<(std::basic_ostream<CharT, TraitsT>& os, const frame& f) {
    return os << boost::stacktrace::to_string(f);
}

}} // namespace boost::stacktrace

BOOST_STACKTRACE_END_MODULE_EXPORT

/// @cond

#include <boost/stacktrace/detail/pop_options.h>

#if !defined(BOOST_STACKTRACE_LINK)
#   if defined(BOOST_STACKTRACE_USE_NOOP)
#       include <boost/stacktrace/detail/frame_noop.ipp>
#   elif defined(BOOST_MSVC) || defined(BOOST_STACKTRACE_USE_WINDBG) || defined(BOOST_STACKTRACE_USE_WINDBG_CACHED)
#       include <boost/stacktrace/detail/frame_msvc.ipp>
#   else
#       include <boost/stacktrace/detail/frame_unwind.ipp>
#   endif
#endif
/// @endcond

#endif // !defined(BOOST_USE_MODULES) || defined(BOOST_STACKTRACE_INTERFACE_UNIT)

#endif // BOOST_STACKTRACE_FRAME_HPP
