// Copyright Antony Polukhin, 2016-2026.
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_STACKTRACE_HPP
#define BOOST_STACKTRACE_HPP

#include <boost/stacktrace/detail/backend_config.hpp>

#if !defined(BOOST_USE_MODULES) || defined(BOOST_STACKTRACE_INTERFACE_UNIT)

#ifdef BOOST_HAS_PRAGMA_ONCE
#   pragma once
#endif

#include <boost/stacktrace/frame.hpp>
#include <boost/stacktrace/stacktrace.hpp>
#include <boost/stacktrace/safe_dump_to.hpp>
#include <boost/stacktrace/this_thread.hpp>

#endif // !defined(BOOST_USE_MODULES) || defined(BOOST_STACKTRACE_INTERFACE_UNIT)

#endif // BOOST_STACKTRACE_HPP
