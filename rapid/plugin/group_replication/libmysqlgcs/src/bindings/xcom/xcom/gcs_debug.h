/* Copyright (c) 2016, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef GCS_DEBUG_H
#define GCS_DEBUG_H

#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/xcom_common.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/x_platform.h"

#ifdef TASK_DBUG_ON
#error "TASK_DBUG_ON already defined"
#else
#define TASK_DBUG_ON 0
#endif

#define TX_FMT "{" SY_FMT_DEF " %lu}"
#define TX_MEM(x) SY_MEM((x).cfg), (x).pc

#include <stdio.h>
#include <stdlib.h>

double task_now();

#ifdef DBGOUT
#error "DBGOUT defined"
#endif

#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/xcom_logger.h"

#ifndef XCOM_STANDALONE
#include "my_compiler.h"
#endif

/**
  Callbacks used in the logging macros.
*/

extern xcom_logger xcom_log;
extern xcom_debugger xcom_debug;
extern xcom_debugger_check xcom_debug_check;


/**
  Prints the logging messages to the console. It is invoked when no logger
  callback was set by an upper layer.
*/
void xcom_default_log(const int64_t l, const char *msg);


/**
  Prints the logging messages to the console. It is invoked when no debugger
  callback was set by an upper layer.
*/
void xcom_default_debug(const char *format, ...)
                         MY_ATTRIBUTE((format(printf, 1, 2)));


/**
  Check whether a set of debug and trace options are enabled. It is invoked
  when no debugger callback was set by an upper layer.
*/
int xcom_default_debug_check(const int64_t options);


/**
  Define the set of debug and trace options that are enabled if there
  is no debugger check injected.
*/
extern int64_t xcom_debug_options;


/**
  Concatenates two strings and returns pointer to last character of final
  string, allowing further concatenations without having to cycle through the
  entire string again.

  @param dest pointer to last character of destination string
  @param size pointer to the number of characters currently added to
    xcom_log_buffer
  @param src pointer to the string to append to dest
  @return pointer to the last character of destination string after appending
    dest, which corresponds to the position of the '\0' character
*/

char *mystrcat(char *dest, int *size, const char *src);

/**
  This function allocates a new string where the format string and optional
  arguments are rendered to.
  Finally, it invokes mystr_cat to concatenate the rendered string to the
  string received in the first parameter.
*/

char *mystrcat_sprintf(char *dest, int *size, const char *format, ...)
    MY_ATTRIBUTE((format(printf, 3, 4)));

#define STR_SIZE 2047

#define GET_GOUT                         \
  char xcom_log_buffer[STR_SIZE + 1];    \
  char *xcom_temp_buf = xcom_log_buffer; \
  int xcom_log_buffer_size = 0;          \
  xcom_log_buffer[0] = 0
#define GET_NEW_GOUT                                                     \
  char *xcom_log_buffer = (char *)malloc((STR_SIZE + 1) * sizeof(char)); \
  char *xcom_temp_buf = xcom_log_buffer;                                 \
  int xcom_log_buffer_size = 0;                                          \
  xcom_log_buffer[0] = 0
#define FREE_GOUT xcom_log_buffer[0] = 0
#define ADD_GOUT(s) \
  xcom_temp_buf = mystrcat(xcom_temp_buf, &xcom_log_buffer_size, s)
#define COPY_AND_FREE_GOUT(s) \
  {                           \
    char *__funny = s;        \
    ADD_GOUT(__funny);        \
    free(__funny);            \
  }
#define ADD_F_GOUT(...) \
  xcom_temp_buf =       \
      mystrcat_sprintf(xcom_temp_buf, &xcom_log_buffer_size, __VA_ARGS__)
#define PRINT_LOUT(level) xcom_log(level, xcom_log_buffer)
#define PRINT_GOUT xcom_debug("%s", xcom_log_buffer)
#define RET_GOUT return xcom_log_buffer

#define G_LOG_LEVEL(level, ...)  \
  {                              \
    GET_GOUT;                    \
    ADD_F_GOUT(__VA_ARGS__);     \
    PRINT_LOUT(level);           \
    FREE_GOUT;                   \
  }

#ifndef XCOM_STANDALONE
#define G_DEBUG_LEVEL(level, ...)     \
  {                                   \
    if (IS_XCOM_DEBUG_WITH(level))    \
    {                                 \
      xcom_debug(__VA_ARGS__);        \
    }                                 \
  }
#else
#define G_DEBUG_LEVEL(level, ...)    \
  {                                  \
    if (IS_XCOM_DEBUG_WITH(level)) { \
      GET_GOUT;                      \
      ADD_F_GOUT(__VA_ARGS__);       \
      PRINT_GOUT;                    \
      FREE_GOUT;                     \
    }                                \
  }
#endif /* XCOM_STANDALONE */

#define g_critical(...) G_LOG_LEVEL(XCOM_LOG_FATAL,__VA_ARGS__)
#define G_ERROR(...)    G_LOG_LEVEL(XCOM_LOG_ERROR,__VA_ARGS__)
#define G_WARNING(...)  G_LOG_LEVEL(XCOM_LOG_WARN,__VA_ARGS__)
#define G_MESSAGE(...)  G_LOG_LEVEL(XCOM_LOG_INFO, __VA_ARGS__)
#define G_INFO(...)     G_LOG_LEVEL(XCOM_LOG_INFO, __VA_ARGS__)
#define G_DEBUG(...)    G_DEBUG_LEVEL(XCOM_DEBUG_BASIC | XCOM_DEBUG_TRACE, __VA_ARGS__)
#define G_TRACE(...)    G_DEBUG_LEVEL(XCOM_DEBUG_TRACE, __VA_ARGS__)
#define IS_XCOM_DEBUG_WITH(level) xcom_debug_check(level)

#if TASK_DBUG_ON
#define DBGOHK(x)                                  \
  do {                                             \
    if (IS_XCOM_DEBUG_WITH(XCOM_DEBUG_TRACE)) {    \
      GET_GOUT;                                    \
      ADD_F_GOUT("%f ", task_now());               \
      NEXP(get_nodeno(get_site_def()), u);         \
      x;                                           \
      PRINT_GOUT;                                  \
      FREE_GOUT;                                   \
    }                                              \
  } while (0)
#define DBGOUT(x)                                  \
  do {                                             \
    if (IS_XCOM_DEBUG_WITH(XCOM_DEBUG_TRACE)) {    \
      GET_GOUT;                                    \
      ADD_F_GOUT("%f ", task_now());               \
      x;                                           \
      PRINT_GOUT;                                  \
      FREE_GOUT;                                   \
   }                                               \
  } while (0)
#define MAY_DBG(x) DBGOUT(x)
#else
#define DBGOHK(x) \
  do {            \
  } while (0)
#define DBGOUT(x) \
  do {            \
  } while (0)
#define MAY_DBG(x) \
  do {             \
  } while (0)
#endif

#define XDBG #error
#define FN            \
  ADD_GOUT(__func__); \
  ADD_F_GOUT(" " __FILE__ ":%d ", __LINE__)
#define PTREXP(x) ADD_F_GOUT(#x ": %p ", (void *)(x))
#define PPUT(x) ADD_F_GOUT("0x%p ", (void *)(x))
#define STREXP(x) ADD_F_GOUT(#x ": %s ", x)
#define STRLIT(x) ADD_GOUT(x)
#define NPUT(x, f) ADD_F_GOUT("%" #f " ", x)
#define NDBG(x, f)      \
  ADD_F_GOUT(#x " = "); \
  NPUT(x, f)
#define NDBG64(x) ADD_F_GOUT(#x " = "); NPUT64(x);
#define NPUT64(x) ADD_F_GOUT("%" PRIu64 " ",x)
#define NEXP(x, f) ADD_F_GOUT(#x ": %" #f " ", x)
#define NUMEXP(x) NEXP(x, d)
#define g_strerror strerror
#define LOUT(pri, x) ADD_F_GOUT(x);
#define SYCEXP(exp)                                                       \
  ADD_F_GOUT(#exp "={%x %" PRIu64 " %u} ", (exp).group_id, ((exp).msgno), \
             ((exp).node))
#define TIDCEXP(exp)                                              \
  ADD_F_GOUT(#exp "={%x %" PRIu64 " %u %u} ", (exp).cfg.group_id, \
             (exp).cfg.msgno, (exp).cfg.node, (exp).pc)
#define TIMECEXP(exp) ADD_F_GOUT(#exp "=%f sec ", (exp))
#define BALCEXP(exp) ADD_F_GOUT(#exp "={%d %d} ", (exp).cnt, (exp).node)

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* GCS_DEBUG_H */
