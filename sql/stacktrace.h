/* Copyright (C) 2000 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifdef	__cplusplus
extern "C" {
#endif

#if HAVE_BACKTRACE && HAVE_BACKTRACE_SYMBOLS && HAVE_CXXABI_H && HAVE_ABI_CXA_DEMANGLE
#define BACKTRACE_DEMANGLE 1
#endif

#if BACKTRACE_DEMANGLE
char *my_demangle(const char *mangled_name, int *status);
#endif

#ifdef TARGET_OS_LINUX
#if defined(HAVE_STACKTRACE) || (defined (__x86_64__) || defined (__i386__) || (defined(__alpha__) && defined(__GNUC__)))
#undef HAVE_STACKTRACE
#define HAVE_STACKTRACE

extern char* __bss_start;
extern char* heap_start;

#define init_stacktrace() do {                                 \
                            heap_start = (char*) &__bss_start; \
                          } while(0);
void print_stacktrace(uchar* stack_bottom, ulong thread_stack);
void safe_print_str(const char* name, const char* val, int max_len);
#endif /* (defined (__i386__) || (defined(__alpha__) && defined(__GNUC__))) */
#endif /* TARGET_OS_LINUX */

/* Define empty prototypes for functions that are not implemented */
#ifndef HAVE_STACKTRACE
#define init_stacktrace() {}
#define print_stacktrace(A,B) {}
#define safe_print_str(A,B,C) {}
#endif /* HAVE_STACKTRACE */

void write_core(int sig);

#ifdef	__cplusplus
}
#endif
