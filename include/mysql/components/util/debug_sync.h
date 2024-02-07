/* Copyright (c) 2023, 2024, Oracle and/or its affiliates.

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

#ifndef DEBUG_SYNC_GUARD
#define DEBUG_SYNC_GUARD

#include "mysql/components/component_implementation.h"
#include "mysql/components/services/mysql_current_thread_reader.h"  // MYSQL_THD
#include "mysql/components/services/mysql_debug_sync_service.h"

#if !defined(NDEBUG)

#define DEBUG_SYNC(name)                                                  \
  {                                                                       \
    extern REQUIRES_SERVICE_PLACEHOLDER(mysql_current_thread_reader);     \
    extern REQUIRES_SERVICE_PLACEHOLDER(mysql_debug_sync_service);        \
    MYSQL_THD thd = nullptr;                                              \
    SERVICE_PLACEHOLDER(mysql_current_thread_reader)->get(&thd);          \
    assert(thd);                                                          \
    SERVICE_PLACEHOLDER(mysql_debug_sync_service)->debug_sync(thd, name); \
  }

#else

#define DEBUG_SYNC(name)

#endif

#endif  // DEBUG_SYNC_GUARD