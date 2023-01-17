/*****************************************************************************

Copyright (c) 2021, 2023, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is also distributed with certain software (including but not
limited to OpenSSL) that is licensed under separate terms, as designated in a
particular file or component or in included license documentation. The authors
of MySQL hereby grant you an additional permission to link the program and
your derivative works with the separately licensed software that they have
included with MySQL.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/
#ifndef JEMALLOC_WIN_INCLUDED
#define JEMALLOC_WIN_INCLUDED

/** @file jemalloc_win.h
 Details for dynamically loading and using jemalloc.dll on Windows */

#ifdef _WIN32
#include <mutex>
#include <string>
#include <vector>
#include "my_loglevel.h"
const constexpr char *jemalloc_dll_name = "jemalloc.dll";
const constexpr char *jemalloc_malloc_function_name = "je_malloc";
const constexpr char *jemalloc_calloc_function_name = "je_calloc";
const constexpr char *jemalloc_realloc_function_name = "je_realloc";
const constexpr char *jemalloc_free_function_name = "je_free";

namespace mysys {
extern bool is_my_malloc_using_jemalloc();
struct LogMessageInfo {
  loglevel m_severity;
  int64_t m_ecode;
  std::string m_message;
};
extern std::vector<LogMessageInfo> fetch_jemalloc_initialization_messages();

const int64_t MY_MALLOC_USING_JEMALLOC_ER = 0;
const int64_t MY_MALLOC_USING_STD_MALLOC_ER = 1;
const int64_t MY_MALLOC_LOADLIBRARY_FAILED_ER = 2;
const int64_t MY_MALLOC_GETPROCADDRESS_FAILED_ER = 3;

namespace detail {
extern void *(*pfn_malloc)(size_t size);
extern void *(*pfn_calloc)(size_t number, size_t size);
extern void *(*pfn_realloc)(void *ptr, size_t size);
extern void (*pfn_free)(void *ptr);
extern std::once_flag init_malloc_pointers_flag;
void init_malloc_pointers();
}  // namespace detail
}  // namespace mysys

#endif  // _WIN32

#endif  // JEMALLOC_WIN_INCLUDED
