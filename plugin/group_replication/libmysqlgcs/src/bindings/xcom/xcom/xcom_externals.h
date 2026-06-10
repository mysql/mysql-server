/* Copyright (c) 2015, 2026, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef XCOM_EXTERNALS
#define XCOM_EXTERNALS

#include <cstdarg>
#include <cstring>
#include <string>
#include <vector>

#if TASK_DBUG_ON

#ifdef XCOM_DBGOUT
#error "XCOM_DBGOUT already defined!"
#else
#define XCOM_DBGOUT(x)                          \
  do {                                          \
    if (IS_XCOM_DEBUG_WITH(XCOM_DEBUG_TRACE)) { \
      GET_GOUT;                                 \
      ADD_F_GOUT("%f ", task_now());            \
      x;                                        \
      PRINT_GOUT;                               \
      FREE_GOUT;                                \
    }                                           \
  } while (0);
#endif  // XCOM_DBGOUT

#ifdef XCOM_IFDBG
#error "XCOM_IFDBG already defined!"
#else
#define XCOM_IFDBG(mask, body)          \
  {                                     \
    if (do_dbg(mask)) XCOM_DBGOUT(body) \
  }
#endif

#else

#define XCOM_DBGOUT(x) \
  do {                 \
  } while (0)
#define XCOM_IFDBG(mask, body)

#endif  // TASK_DBUG_ON

#endif  // XCOM_EXTERNALS
