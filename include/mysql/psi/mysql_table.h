/* Copyright (c) 2010, Oracle and/or its affiliates. All rights reserved.   

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software Foundation,
  51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#ifndef MYSQL_TABLE_H
#define MYSQL_TABLE_H

/**
  @file mysql/psi/mysql_table.h
  Instrumentation helpers for table io.
*/

#include "mysql/psi/psi.h"

/**
  @defgroup Table_instrumentation Table Instrumentation
  @ingroup Instrumentation_interface
  @{
*/

/**
  @def MYSQL_START_TABLE_WAIT
  Instrumentation helper for table waits.
  This instrumentation marks the start of a wait event.
  @param STATE The locker state
  @param PSI The instrumented table
  @param OP The table operation to be performed
  @param INDEX The table index used if any, or MAY_KEY.
  @param FLAGS Per table operation flags.
  @sa MYSQL_END_TABLE_WAIT.
*/
#ifdef HAVE_PSI_INTERFACE
  #define MYSQL_START_TABLE_WAIT(STATE, PSI, OP, INDEX, FLAGS) \
    inline_mysql_start_table_wait(STATE, PSI, OP, INDEX, FLAGS, __FILE__, __LINE__)
#else
  #define MYSQL_START_TABLE_WAIT(STATE, PSI, OP, INDEX, FLAGS) \
    NULL
#endif

/**
  @def MYSQL_END_TABLE_WAIT
  Instrumentation helper for table waits.
  This instrumentation marks the end of a wait event.
  @sa MYSQL_START_TABLE_WAIT.
*/
#ifdef HAVE_PSI_INTERFACE
  #define MYSQL_END_TABLE_WAIT(L) \
    inline_mysql_end_table_wait(L)
#else
  #define MYSQL_END_TABLE_WAIT(L) \
    do {} while (0)
#endif

#ifdef HAVE_PSI_INTERFACE
/**
  Instrumentation calls for MYSQL_START_TABLE_WAIT.
  @sa MYSQL_START_TABLE_WAIT.
*/
static inline struct PSI_table_locker *
inline_mysql_start_table_wait(PSI_table_locker_state *state,
                              struct PSI_table *psi, enum PSI_table_operation op,
                              uint index, ulong flags,
                              const char *src_file, int src_line)
{
  struct PSI_table_locker *locker= NULL;
  if (likely(PSI_server && psi))
  {
    locker= PSI_server->get_thread_table_locker(state, psi, op, flags);
    if (likely(locker != NULL))
      PSI_server->start_table_wait(locker, index, src_file, src_line);
  }
  return locker;
}

/**
  Instrumentation calls for MYSQL_END_TABLE_WAIT.
  @sa MYSQL_END_TABLE_WAIT.
*/
static inline void
inline_mysql_end_table_wait(struct PSI_table_locker *locker)
{
  if (likely(locker != NULL))
    PSI_server->end_table_wait(locker);
}
#endif

/** @} (end of group Table_instrumentation) */

#endif

