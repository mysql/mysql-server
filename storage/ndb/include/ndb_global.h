/*
   Copyright (c) 2004, 2017, Oracle and/or its affiliates. All rights reserved.

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

#include <errno.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_systime.h"
#include <mysql/service_my_snprintf.h>
#include <mysql/service_mysql_alloc.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef _WIN32
#include <process.h>
#endif

/*
  Custom version of standard offsetof() macro which can be used to get
  offsets of members in class for non-POD types (according to the current
  version of C++ standard offsetof() macro can't be used in such cases and
  attempt to do so causes warnings to be emitted, OTOH in many cases it is
  still OK to assume that all instances of the class has the same offsets
  for the same members).

  This is temporary solution which should be removed once File_parser class
  and related routines are refactored.
*/

#define my_offsetof(TYPE, MEMBER) \
        ((size_t)((char *)&(((TYPE *)0x10)->MEMBER) - (char*)0x10))

#if defined __GNUC__
# define ATTRIBUTE_FORMAT(style, m, n) MY_ATTRIBUTE((format(style, m, n)))
#else
# define ATTRIBUTE_FORMAT(style, m, n)
#endif

#ifdef HAVE_NDB_CONFIG_H
#include "ndb_config.h"
#endif

#include <mysql_com.h>
#include <ndb_types.h>

#ifndef NDB_PORT
/* Default port used by ndb_mgmd */
#define NDB_PORT 1186
#endif

#if defined(_WIN32)
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

#ifdef  __cplusplus
#include <new>
#endif

#include "ndb_init.h"

#ifndef PATH_MAX
#define PATH_MAX 1024
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

#define NDB_STATIC_ASSERT(expr) static_assert(expr, #expr)

#if defined(_WIN32) && (_MSC_VER > 1500)
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
 *  MY_ATTRIBUTE((noreturn)) was introduce in gcc 2.5
 */
#ifdef __GNUC__
#define ATTRIBUTE_NORETURN MY_ATTRIBUTE((noreturn))
#else
#define ATTRIBUTE_NORETURN
#endif

/**
 *  MY_ATTRIBUTE((noinline)) was introduce in gcc 3.1
 */
#ifdef __GNUC__
#define ATTRIBUTE_NOINLINE MY_ATTRIBUTE((noinline))
#else
#define ATTRIBUTE_NOINLINE
#endif

/**
 * sizeof cacheline (in bytes)
 *
 * TODO: Add configure check...
 */
#define NDB_CL 64

/**
 * Pad to NDB_CL size
 */
#define NDB_CL_PADSZ(x) (NDB_CL - ((x) % NDB_CL))

/*
 * require is like a normal assert, only it's always on (eg. in release)
 */
C_MODE_START
typedef int(*RequirePrinter)(const char *fmt, ...)
  ATTRIBUTE_FORMAT(printf, 1, 2);
void require_failed(int exitcode, RequirePrinter p,
                    const char* expr, const char* file, int line)
                    ATTRIBUTE_NORETURN;
int ndbout_printer(const char * fmt, ...)
  ATTRIBUTE_FORMAT(printf, 1, 2);
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

struct LinearSectionPtr
{
  Uint32 sz;
  Uint32 * p;
};

struct SegmentedSectionPtrPOD
{
  Uint32 sz;
  Uint32 i;
  struct SectionSegment * p;

#ifdef __cplusplus
  void setNull() { p = 0;}
  bool isNull() const { return p == 0;}
  inline SegmentedSectionPtrPOD& assign(struct SegmentedSectionPtr&);
#endif
};

struct SegmentedSectionPtr
{
  Uint32 sz;
  Uint32 i;
  struct SectionSegment * p;

#ifdef __cplusplus
  SegmentedSectionPtr() {}
  SegmentedSectionPtr(Uint32 sz_arg, Uint32 i_arg,
                      struct SectionSegment *p_arg)
    :sz(sz_arg), i(i_arg), p(p_arg)
  {}
  SegmentedSectionPtr(const SegmentedSectionPtrPOD & src)
    :sz(src.sz), i(src.i), p(src.p)
  {}

  void setNull() { p = 0;}
  bool isNull() const { return p == 0;}
#endif
};

#ifdef __cplusplus
inline
SegmentedSectionPtrPOD&
SegmentedSectionPtrPOD::assign(struct SegmentedSectionPtr& src)
{
  this->i = src.i;
  this->p = src.p;
  this->sz = src.sz;
  return *this;
}
#endif

/* Abstract interface for iterating over
 * words in a section
 */
#ifdef __cplusplus
struct GenericSectionIterator
{
  virtual ~GenericSectionIterator() {};
  virtual void reset()=0;
  virtual const Uint32* getNextWords(Uint32& sz)=0;
};
#else
struct GenericSectionIterator;
#endif

struct GenericSectionPtr
{
  Uint32 sz;
  struct GenericSectionIterator* sectionIter;
};

#endif
