/*
   Copyright (c) 2001, 2011, Oracle and/or its affiliates.

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

#ifndef _my_stacktrace_h_
#define _my_stacktrace_h_

#include <my_global.h>

#ifdef TARGET_OS_LINUX
#if defined (__x86_64__) || defined (__i386__) || \
    (defined(__alpha__) && defined(__GNUC__))
#define HAVE_STACKTRACE 1
#endif
#elif defined(__WIN__) || defined(HAVE_PRINTSTACK)
#define HAVE_STACKTRACE 1
#endif

#if HAVE_BACKTRACE && (HAVE_BACKTRACE_SYMBOLS || HAVE_BACKTRACE_SYMBOLS_FD)
#undef HAVE_STACKTRACE
#define HAVE_STACKTRACE 1
#endif

#define HAVE_WRITE_CORE

#if HAVE_BACKTRACE && HAVE_BACKTRACE_SYMBOLS && \
    HAVE_CXXABI_H && HAVE_ABI_CXA_DEMANGLE && \
    HAVE_WEAK_SYMBOL
#define BACKTRACE_DEMANGLE 1
#endif

C_MODE_START

#if defined(HAVE_STACKTRACE) || defined(HAVE_BACKTRACE)
void my_init_stacktrace();
void my_print_stacktrace(uchar* stack_bottom, ulong thread_stack);
void my_safe_print_str(const char* val, int max_len);
void my_write_core(int sig);
#if BACKTRACE_DEMANGLE
char *my_demangle(const char *mangled_name, int *status);
#endif /* BACKTRACE_DEMANGLE */
#ifdef __WIN__
void my_set_exception_pointers(EXCEPTION_POINTERS *ep);
#endif /* __WIN__ */
#else
#define my_init_stacktrace() do { } while(0)
#endif /* ! (defined(HAVE_STACKTRACE) || defined(HAVE_BACKTRACE)) */

#ifndef _WIN32
#define MY_ADDR_RESOLVE_FORK
#endif

#if defined(HAVE_BFD_H) || defined(MY_ADDR_RESOLVE_FORK)
#define HAVE_MY_ADDR_RESOLVE 1
#endif

typedef struct {
  const char *file;
  const char *func;
  uint line;
} my_addr_loc;

#ifdef HAVE_MY_ADDR_RESOLVE
int my_addr_resolve(void *ptr, my_addr_loc *loc);
const char *my_addr_resolve_init();
#else
#define my_addr_resolve_init()  (0)
#define my_addr_resolve(A,B)    (1)
#endif

#ifdef HAVE_WRITE_CORE
void my_write_core(int sig);
#endif

/**
  A (very) limited version of snprintf, which writes the result to STDERR.
  @sa my_safe_snprintf
  Implemented with simplicity, and async-signal-safety in mind.
  @note Has an internal buffer capacity of 512 bytes,
  which should suffice for our signal handling routines.
*/
size_t my_safe_printf_stderr(const char* fmt, ...)
  ATTRIBUTE_FORMAT(printf, 1, 2);

/**
  Writes up to count bytes from buffer to STDERR.
  Implemented with simplicity, and async-signal-safety in mind.
  @param   buf   Buffer containing data to be written.
  @param   count Number of bytes to write.
  @returns Number of bytes written.
*/
size_t my_write_stderr(const void *buf, size_t count);

C_MODE_END

#endif /* _my_stacktrace_h_ */
