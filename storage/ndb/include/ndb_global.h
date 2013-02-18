/*
   Copyright (c) 2004, 2010, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef NDB_GLOBAL_H
#define NDB_GLOBAL_H

#ifdef _WIN32
/* Workaround for Bug#32082: VOID refdefinition results in compile errors */
#ifndef DONT_DEFINE_VOID
#define DONT_DEFINE_VOID
#endif
#endif

#include <my_global.h>

#ifdef HAVE_NDB_CONFIG_H
#include "ndb_config.h"
#endif

#include <mysql_com.h>
#include <ndb_types.h>

#ifndef NDB_PORT
/* Default port used by ndb_mgmd */
#define NDB_PORT 1186
#endif

#if defined(_WIN32) || defined(_WIN64) || defined(__WIN32__) || defined(WIN32)
#define NDB_WIN32 1
#define NDB_WIN 1
#define PATH_MAX 256
#define DIR_SEPARATOR "\\"

/* Disable a few compiler warnings on Windows */
/* 4355: 'this': used in base member initializer list */
#pragma warning(disable: 4355)

#else
#undef NDB_WIN32
#undef NDB_WIN
#define DIR_SEPARATOR "/"
#endif

#if ! (NDB_SIZEOF_CHAR == SIZEOF_CHAR)
#error "Invalid define for Uint8"
#endif

#if ! (NDB_SIZEOF_INT == SIZEOF_INT)
#error "Invalid define for Uint32"
#endif

#if ! (NDB_SIZEOF_LONG_LONG == SIZEOF_LONG_LONG)
#error "Invalid define for Uint64"
#endif

#include <signal.h>

#ifdef _AIX
#undef _H_STRINGS
#endif
#include <m_string.h>

#ifndef NDB_REMOVE_BZERO
/*
  Make it possible to use bzero in NDB although
  MySQL headers redefines it to an invalid symbol
*/
#ifdef bzero
#undef bzero
#endif

#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif

#if !defined(bzero) && !defined(HAVE_BZERO)
#define bzero(A,B) memset((A),0,(B))
#endif
#endif

#include <m_ctype.h>
#include <ctype.h>

#ifdef HAVE_STDARG_H
#include <stdarg.h>
#endif

#ifdef TIME_WITH_SYS_TIME
#include <time.h>
#endif

#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif

#include <sys/stat.h>

#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif

#ifdef HAVE_SYS_RESOURCE_H
#include <sys/resource.h>
#endif

#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif

#ifdef HAVE_SYS_MMAN_H
#include <sys/mman.h>
#endif

#ifndef HAVE_STRDUP
extern char * strdup(const char *s);
#endif

static const char table_name_separator =  '/';

#if defined(_AIX) || defined(WIN32) || defined(NDB_VC98)
#define STATIC_CONST(x) enum { x }
#else
#define STATIC_CONST(x) static const Uint32 x
#endif

#ifdef  __cplusplus
extern "C" {
#endif
	
#include <assert.h>

#ifdef  __cplusplus
}
#endif

#include "ndb_init.h"

#ifndef PATH_MAX
#define PATH_MAX 1024
#endif

#if defined(_lint) || defined(FORCE_INIT_OF_VARS)
#define LINT_SET_PTR = {0,0}
#else
#define LINT_SET_PTR
#endif

#ifndef MIN
#define MIN(x,y) (((x)<(y))?(x):(y))
#endif

#ifndef MAX
#define MAX(x,y) (((x)>(y))?(x):(y))
#endif

/*
  Dont allow use of min() or max() macros
   - in order to enforce forward compatibilty
*/

#ifdef min
#undef min
#endif

#ifdef max
#undef max
#endif

#define NDB_O_DIRECT_WRITE_ALIGNMENT 512

#ifndef STATIC_ASSERT
#if defined VM_TRACE
/**
 * Compile-time assert for use from procedure body
 * Zero length array not allowed in C
 * Add use of array to avoid compiler warning
 */
#define STATIC_ASSERT(expr) { char a_static_assert[(expr)? 1 : 0] = {'\0'}; if (a_static_assert[0]) {}; }
#else
#define STATIC_ASSERT(expr)
#endif
#endif

#define NDB_ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))


/*
  NDB_STATIC_ASSERT(expr)
   - Check coding assumptions during compile time
     by laying out code that will generate a compiler error
     if the expression is false.
*/

#if (_MSC_VER > 1500) || (defined __GXX_EXPERIMENTAL_CXX0X__)

/*
  Prefer to use the 'static_assert' function from C++0x
  to get best error message
*/
#define NDB_STATIC_ASSERT(expr) static_assert(expr, #expr)

#else

/*
  Fallback to use home grown solution
  (i.e use mysys version)
*/

#define NDB_STATIC_ASSERT(expr) compile_time_assert(expr)

#endif


#if (_MSC_VER > 1500)
#define HAVE___HAS_TRIVIAL_CONSTRUCTOR
#define HAVE___IS_POD
#endif

#ifdef HAVE___HAS_TRIVIAL_CONSTRUCTOR
#define ASSERT_TYPE_HAS_CONSTRUCTOR(x)     \
  NDB_STATIC_ASSERT(!__has_trivial_constructor(x))
#else
#define ASSERT_TYPE_HAS_CONSTRUCTOR(x)
#endif

/**
 * visual studio is stricter than gcc for __is_pod, settle for __has_trivial_constructor
 *  until we really really made all signal data classes POD
 *
 * UPDATE: also gcc fails to compile our code with gcc4.4.3
 */
#ifdef HAVE___HAS_TRIVIAL_CONSTRUCTOR
#define NDB_ASSERT_POD(x) \
  NDB_STATIC_ASSERT(__has_trivial_constructor(x))
#else
#define NDB_ASSERT_POD(x)
#endif

/**
 *  __attribute__((noreturn)) was introduce in gcc 2.5
 */
#if (GCC_VERSION >= 2005)
#define ATTRIBUTE_NORETURN __attribute__((noreturn))
#else
#define ATTRIBUTE_NORETURN
#endif

/**
 *  __attribute__((noinline)) was introduce in gcc 3.1
 */
#if (GCC_VERSION >= 3001)
#define ATTRIBUTE_NOINLINE __attribute__((noinline))
#else
#define ATTRIBUTE_NOINLINE
#endif

/**
 * Pad to NDB_CL size
 */
#define NDB_CL_PADSZ(x) (NDB_CL - ((x) % NDB_CL))

/*
 * require is like a normal assert, only it's always on (eg. in release)
 */
C_MODE_START
/** see below */
typedef int(*RequirePrinter)(const char *fmt, ...);
void require_failed(int exitcode, RequirePrinter p,
                    const char* expr, const char* file, int line);
int ndbout_printer(const char * fmt, ...);
C_MODE_END
/*
 *  this allows for an exit() call if exitcode is not zero
 *  and takes a Printer to print the error
 */
#define require_exit_or_core_with_printer(v, exitcode, printer) \
  do { if (likely(!(!(v)))) break;                                    \
       require_failed((exitcode), (printer), #v, __FILE__, __LINE__); \
  } while (0)

/*
 *  this allows for an exit() call if exitcode is not zero
*/
#define require_exit_or_core(v, exitcode) \
       require_exit_or_core_with_printer((v), (exitcode), 0)

/*
 * this require is like a normal assert.  (only it's always on)
*/
#define require(v) require_exit_or_core_with_printer((v), 0, 0)

#endif
