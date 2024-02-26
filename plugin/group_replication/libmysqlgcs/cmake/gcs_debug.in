/* Copyright (c) 2016, 2023, Oracle and/or its affiliates.

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

#include "xcom/xcom_common.h"

#include "xcom/x_platform.h"

#ifdef TASK_DBUG_ON
#error "TASK_DBUG_ON already defined"
#else
#define TASK_DBUG_ON 0
#endif

#define TX_FMT "{" SY_FMT_DEF " %" PRIu32 "}"
#define TX_MEM(x) SY_MEM((x).cfg), (x).pc

#include <stdio.h>
#include <stdlib.h>

double task_now();

#ifdef DBGOUT
#error "DBGOUT defined"
#endif

#include "xcom/xcom_logger.h"

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

#define G_LOG_LEVEL(level, ...) \
  {                             \
    GET_GOUT;                   \
    ADD_F_GOUT(__VA_ARGS__);    \
    PRINT_LOUT(level);          \
    FREE_GOUT;                  \
  }

#ifndef XCOM_STANDALONE
#define G_DEBUG_LEVEL(level, ...)    \
  {                                  \
    if (IS_XCOM_DEBUG_WITH(level)) { \
      xcom_debug(__VA_ARGS__);       \
    }                                \
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

#define g_critical(...) G_LOG_LEVEL(XCOM_LOG_FATAL, __VA_ARGS__)
#define G_ERROR(...) G_LOG_LEVEL(XCOM_LOG_ERROR, __VA_ARGS__)
#define G_WARNING(...) G_LOG_LEVEL(XCOM_LOG_WARN, __VA_ARGS__)
#define G_MESSAGE(...) G_LOG_LEVEL(XCOM_LOG_INFO, __VA_ARGS__)
#define G_INFO(...) G_LOG_LEVEL(XCOM_LOG_INFO, __VA_ARGS__)
#define G_DEBUG(...) \
  G_DEBUG_LEVEL(XCOM_DEBUG_BASIC | XCOM_DEBUG_TRACE, __VA_ARGS__)
#define G_TRACE(...) G_DEBUG_LEVEL(XCOM_DEBUG_TRACE, __VA_ARGS__)
#define IS_XCOM_DEBUG_WITH(level) xcom_debug_check(level)

#ifdef IDENTIFY
#error "IDENTIFY already defined!"
#else
#define IDENTIFY
#endif

#ifdef DBG_IDENTIFY
#error "DBG_IDENTIFY already defined!"
#else
#define DBG_IDENTIFY
#endif

#define BIT(n) (1L << n)

enum xcom_dbg_type {
  D_NONE = 0,
  D_TASK = BIT(0),
  D_BASE = BIT(1),
  D_FSM = BIT(2),
  D_TRANSPORT = BIT(3),
  D_PROPOSE = BIT(4),
  D_DISPATCH = BIT(5),
  D_SEMA = BIT(6),
  D_XDR = BIT(7),
  D_STORE = BIT(8),
  D_EXEC = BIT(9),
  D_DETECT = BIT(10),
  D_ALLOC = BIT(11),
  D_FILEOP = BIT(12),
  D_CACHE = BIT(13),
  D_CONS = BIT(14),
  D_BUG = ~0L
};
typedef enum xcom_dbg_type xcom_dbg_type;

enum { DBG_STACK_SIZE = 256 };
extern long xcom_debug_mask;
extern long xcom_dbg_stack[DBG_STACK_SIZE];
extern int xcom_dbg_stack_top;

static inline int do_dbg(xcom_dbg_type x) { return (x & xcom_debug_mask) != 0; }
static inline void set_dbg(xcom_dbg_type x) { xcom_debug_mask |= x; }
static inline void unset_dbg(xcom_dbg_type x) { xcom_debug_mask &= ~x; }
static inline long get_dbg() { return xcom_debug_mask; }

static inline void push_dbg(long x) {
  if (xcom_dbg_stack_top < DBG_STACK_SIZE) {
    xcom_dbg_stack[xcom_dbg_stack_top] = xcom_debug_mask;
    xcom_dbg_stack_top++;
    xcom_debug_mask = x;
  }
}

static inline void pop_dbg() {
  if (xcom_dbg_stack_top > 0) {
    xcom_dbg_stack_top--;
    xcom_debug_mask = xcom_dbg_stack[xcom_dbg_stack_top];
  }
}

#ifdef INFO
#error "INFO already defined!"
#else
#define INFO(x)                    \
  do {                             \
    GET_GOUT;                      \
    ADD_F_GOUT("%f ", task_now()); \
    x;                             \
    PRINT_LOUT(XCOM_LOG_INFO);     \
    FREE_GOUT;                     \
  } while (0)
#endif

#if TASK_DBUG_ON

#define DBGOUT(x)                               \
  do {                                          \
    if (IS_XCOM_DEBUG_WITH(XCOM_DEBUG_TRACE)) { \
      GET_GOUT;                                 \
      ADD_F_GOUT("%f ", task_now());            \
      x;                                        \
      PRINT_GOUT;                               \
      FREE_GOUT;                                \
    }                                           \
  } while (0)
#define NEW_DBG(x)           \
  long dbg_save = get_dbg(); \
  set_dbg(x)
#define RESTORE_DBG set_dbg(dbg_save)

#ifdef IFDBG
#error "IFDBG already defined!"
#else
#define IFDBG(mask, body)           \
  {                                 \
    if (do_dbg(mask)) DBGOUT(body); \
  }
#endif

#ifdef DBGOUT_ASSERT
#error "DBGOUT_ASSERT already defined"
#else
#define DBGOUT_ASSERT(expr, dbginfo) \
  if (!(expr)) {                     \
    GET_GOUT;                        \
    FN;                              \
    dbginfo;                         \
    PRINT_LOUT(XCOM_LOG_ERROR);      \
    FREE_GOUT;                       \
    abort();                         \
  }
#endif

#else

#define DBGOUT(x) \
  do {            \
  } while (0)
#define DBGOUT_ASSERT(expr, dbginfo)
#define NEW_DBG(x)
#define IFDBG(mask, body)
#endif

#include <sys/types.h>
#ifndef _WIN32
#include <unistd.h>
#endif

static inline int xpid() {
  static int pid = 0;
  if (!pid) pid = getpid();
  return pid;
}

#ifdef _WIN32
static inline char const *fixpath(char const *x) {
  char const *s = strrchr(x, '\\');
  return s ? s + 1 : x;
}
#else
static inline char const *fixpath(char const *x) {
  char const *s = strrchr(x, '/');
  return s ? s + 1 : x;
}
#endif

extern uint32_t get_my_xcom_id();

#define XDBG #error
#define FN                                                          \
  ADD_GOUT(__func__);                                               \
  ADD_F_GOUT(" pid %d xcom_id %x %s:%d ", xpid(), get_my_xcom_id(), \
             fixpath(__FILE__), __LINE__)
#define PTREXP(x) ADD_F_GOUT(#x ": %p ", (void const*)(x))
#define CONSTPTREXP(x) PTREXP(x)
#define PPUT(x) ADD_F_GOUT("0x%p ", (void *)(x))
#define STREXP(x) ADD_F_GOUT(#x ": %s ", x)
#define STRLIT(x) ADD_GOUT(x)
#define NPUT(x, f) ADD_F_GOUT("%" #f " ", x)
#define NDBG(x, f)      \
  ADD_F_GOUT(#x " = "); \
  NPUT(x, f)
#define NDBG64(x)       \
  ADD_F_GOUT(#x " = "); \
  NPUT64(x);
#define NPUT64(x) ADD_F_GOUT("%" PRIu64 " ", x)
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

#endif /* GCS_DEBUG_H */
