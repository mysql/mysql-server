/* Copyright (c) 2001, 2016, Oracle and/or its affiliates. All rights reserved.

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

C_MODE_START

/*
  HAVE_BACKTRACE - Linux
  HAVE_PRINTSTACK - Solaris
  _WIN32 - Windows
  Missing: FreeBSD
*/
#if defined(HAVE_BACKTRACE) || defined(HAVE_PRINTSTACK) || defined(_WIN32)
#define HAVE_STACKTRACE 1
void my_init_stacktrace();
void my_print_stacktrace(uchar* stack_bottom, ulong thread_stack);
void my_safe_puts_stderr(const char* val, size_t max_len);
#if HAVE_BACKTRACE && HAVE_ABI_CXA_DEMANGLE
char *my_demangle(const char *mangled_name, int *status);
#endif
#ifdef _WIN32
void my_set_exception_pointers(EXCEPTION_POINTERS *ep);
void my_create_minidump(const char *name, HANDLE process, DWORD pid);
#endif
#endif /* HAVE_BACKTRACE || HAVE_PRINTSTACK || _WIN32 */

void my_write_core(int sig);



/**
  Async-signal-safe utility functions used by signal handler routines.
  Declared here in order to unit-test them.
  These are not general-purpose, but tailored to the signal handling routines.
*/
/**
  Converts a longlong value to string.
  @param   base 10 for decimal, 16 for hex values (0..9a..f)
  @param   val  The value to convert
  @param   buf  Assumed to point to the *end* of the buffer.
  @returns Pointer to the first character of the converted string.
           Negative values:
           for base-10 the return string will be prepended with '-'
           for base-16 the return string will contain 16 characters
  Implemented with simplicity, and async-signal-safety in mind.
*/
char *my_safe_itoa(int base, longlong val, char *buf);

/**
  Converts a ulonglong value to string.
  @param   base 10 for decimal, 16 for hex values (0..9a..f)
  @param   val  The value to convert
  @param   buf  Assumed to point to the *end* of the buffer.
  @returns Pointer to the first character of the converted string.
  Implemented with simplicity, and async-signal-safety in mind.
*/
char *my_safe_utoa(int base, ulonglong val, char *buf);

/**
  A (very) limited version of snprintf.
  @param   to   Destination buffer.
  @param   n    Size of destination buffer.
  @param   fmt  printf() style format string.
  @returns Number of bytes written, including terminating '\0'
  Supports 'd' 'i' 'u' 'x' 'p' 's' conversion.
  Supports 'l' and 'll' modifiers for integral types.
  Does not support any width/precision.
  Implemented with simplicity, and async-signal-safety in mind.
*/
size_t my_safe_snprintf(char* to, size_t n, const char* fmt, ...)
  MY_ATTRIBUTE((format(printf, 3, 4)));

/**
  A (very) limited version of snprintf, which writes the result to STDERR.
  @sa my_safe_snprintf
  Implemented with simplicity, and async-signal-safety in mind.
  @note Has an internal buffer capacity of 512 bytes,
  which should suffice for our signal handling routines.
*/
size_t my_safe_printf_stderr(const char* fmt, ...)
  MY_ATTRIBUTE((format(printf, 1, 2)));

/**
  Writes up to count bytes from buffer to STDERR.
  Implemented with simplicity, and async-signal-safety in mind.
  @param   buf   Buffer containing data to be written.
  @param   count Number of bytes to write.
  @returns Number of bytes written.
*/
size_t my_write_stderr(const void *buf, size_t count);

/**
  Writes system time to STDERR without allocating new memory.
*/
void my_safe_print_system_time();

C_MODE_END

#endif /* _my_stacktrace_h_ */
