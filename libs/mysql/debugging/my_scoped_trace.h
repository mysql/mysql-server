// Copyright (c) 2025, 2026, Oracle and/or its affiliates.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2.0,
// as published by the Free Software Foundation.
//
// This program is designed to work with certain software (including
// but not limited to OpenSSL) that is licensed under separate terms,
// as designated in a particular file or component or in included license
// documentation.  The authors of MySQL hereby grant you an additional
// permission to link the program and your derivative works with the
// separately licensed software that they have either included with
// the program or referenced in the documentation.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License, version 2.0, for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

#ifndef MYSQL_DEBUGGING_MY_SCOPED_TRACE_H
#define MYSQL_DEBUGGING_MY_SCOPED_TRACE_H

/// @file
/// Experimental API header

#include <gtest/gtest.h>  // SCOPED_TRACE
#include "mysql/strconv/strconv.h"

/// @addtogroup GroupLibsMysqlDebugging
/// @{

// Use macros, because SCOPED_TRACE is a macro that relies on __FILE__ and
// __LINE__
// NOLINTBEGIN(cppcoreguidelines-macro-usage)

/// "Thread-safe", multi-argument replacement for SCOPED_TRACE.
///
/// On some platforms, including Solaris, GTest does not define
/// GTEST_IS_THREADSAFE. Then, use of SCOPED_TRACE in multiple threads results
/// in random crashes because it compiles and runs without warnings but does not
/// synchronize between threads.
///
/// To avoid random crashes, use MY_SCOPED_TRACE instead. The downside is that
/// on those platforms, the "scoped traces" are empty, i.e., you see less debug
/// info when a test assertion fails.
///
/// SCOPED_TRACE only takes one argument. Sometimes it is convenient to pass
/// multiple arguments. This macro accepts multiple arguments and concatenates
/// them using mysql::strconv::throwing::concat.
#ifdef GTEST_IS_THREADSAFE
#define MY_SCOPED_TRACE(...)                     \
  SCOPED_TRACE(mysql::strconv::throwing::concat( \
      mysql::strconv::Debug_format{}, __VA_ARGS__))
#else
// "make use" of the parameters, so that we do not get -Werror=unused-parameter
// errors in functions where `SCOPED_TRACE(x)` is the only use of `x`.
#define MY_SCOPED_TRACE(...) ([](auto &&...) {}(__VA_ARGS__))
#endif

// NOLINTEND(cppcoreguidelines-macro-usage)

// addtogroup GroupLibsMysqlDebugging
/// @}

#endif  // ifndef MYSQL_DEBUGGING_MY_SCOPED_TRACE_H
