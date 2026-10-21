// Copyright Antony Polukhin, 2016-2026.
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_STACKTRACE_STACKTRACE_FWD_HPP
#define BOOST_STACKTRACE_STACKTRACE_FWD_HPP

#include <boost/stacktrace/detail/backend_config.hpp>

#if !defined(BOOST_USE_MODULES) || defined(BOOST_STACKTRACE_INTERFACE_UNIT)

#if !defined(BOOST_STACKTRACE_INTERFACE_UNIT) && !defined(BOOST_STACKTRACE_USE_STD_MODULE)
#include <cstddef>
#include <memory>
#endif // !defined(BOOST_STACKTRACE_INTERFACE_UNIT) && !defined(BOOST_STACKTRACE_USE_STD_MODULE)

/// @file stacktrace_fwd.hpp This header contains only forward declarations of
/// boost::stacktrace::frame, boost::stacktrace::basic_stacktrace, boost::stacktrace::stacktrace
/// and does not include any other Boost headers.

BOOST_STACKTRACE_BEGIN_MODULE_EXPORT

/// @cond
namespace boost { namespace stacktrace {

class frame;
template <class Allocator = std::allocator<frame> > class basic_stacktrace;
typedef basic_stacktrace<> stacktrace;

}} // namespace boost::stacktrace
/// @endcond

BOOST_STACKTRACE_END_MODULE_EXPORT

#endif // !defined(BOOST_USE_MODULES) || defined(BOOST_STACKTRACE_INTERFACE_UNIT)

#endif // BOOST_STACKTRACE_STACKTRACE_FWD_HPP
