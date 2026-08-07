// Copyright Antony Polukhin, 2025-2026.
// Copyright Fedor Osetrov, 2025-2026.
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_STACKTRACE_DETAIL_BACKEND_CONFIG_HPP
#define BOOST_STACKTRACE_DETAIL_BACKEND_CONFIG_HPP

#if !defined(BOOST_STACKTRACE_INTERFACE_UNIT)
# include <boost/config.hpp>
# ifdef BOOST_HAS_PRAGMA_ONCE
#  pragma once
# endif
#endif

#include "boost/stacktrace/detail/config.hpp"

#if defined(BOOST_USE_MODULES) && !defined(BOOST_STACKTRACE_INTERFACE_UNIT) && !defined(BOOST_STACKTRACE_INTERNAL_BUILD_LIBS)
    import boost.stacktrace.BOOST_STACKTRACE_BACKEND_MODULE;
#endif

#endif
