/* Copyright (c) 2013, Monty Program Ab.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef MYSQL_SERVICE_KILL_STATEMENT_INCLUDED
#define MYSQL_SERVICE_KILL_STATEMENT_INCLUDED

/**
  @file
  This service provides functions that allow plugins to support
  the KILL statement.

  In MySQL support for the KILL statement is cooperative. The KILL
  statement only sets a "killed" flag. This function returns the value
  of that flag.  A thread should check it often, especially inside
  time-consuming loops, and gracefully abort the operation if it is
  non-zero.

  thd_is_killed(thd)
  @return 0 - no KILL statement was issued, continue normally
  @return 1 - there was a KILL statement, abort the execution.

  thd_kill_level(thd)
  @return thd_kill_levels_enum values
*/

#ifdef __cplusplus
extern "C" {
#endif

enum thd_kill_levels {
  THD_IS_NOT_KILLED=0,
  THD_ABORT_SOFTLY=50, /**< abort when possible, don't leave tables corrupted */
  THD_ABORT_ASAP=100,  /**< abort asap */
};

extern struct kill_statement_service_st {
  enum thd_kill_levels (*thd_kill_level_func)(const MYSQL_THD);
} *thd_kill_statement_service;

/* backward compatibility helper */
#define thd_killed(THD)   (thd_kill_level(THD) == THD_ABORT_ASAP)

#ifdef MYSQL_DYNAMIC_PLUGIN

#define thd_kill_level(THD) \
        thd_kill_statement_service->thd_kill_level_func(THD)

#else

enum thd_kill_levels thd_kill_level(const MYSQL_THD);

#endif

#ifdef __cplusplus
}
#endif

#endif

