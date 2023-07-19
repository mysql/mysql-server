/*
   Copyright (c) 2004, 2023, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

/*
 * !! DO NOT ADD ANYTHING TO THIS FILE !!
 *
 * Header files should be included in source files that needs them.
 * That is, follow IWYU (include-what-you-use).
 *
 * New symbols should be added in other relevant header files.
 */

#ifndef NDB_GLOBAL_H
#define NDB_GLOBAL_H

#include <assert.h>
#include <errno.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include "my_config.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include <mysql/service_mysql_alloc.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
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

#ifdef HAVE_NDB_CONFIG_H
#include "ndb_config.h"
#endif

#include <mysql_com.h>
#include <ndb_types.h>

#ifndef NDB_PORT
/* Default port used by ndb_mgmd */
#define NDB_PORT 1186
#endif

#ifdef _WIN32
#define DIR_SEPARATOR "\\"
#else
#define DIR_SEPARATOR "/"
#endif

#if defined(_WIN32)
#define PATH_MAX 256

/* Disable a few compiler warnings on Windows */
/* 4355: 'this': used in base member initializer list */
#pragma warning(disable: 4355)

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
#include "m_string.h"

#ifdef HAVE_STDARG_H
#include <stdarg.h>
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

static const char table_name_separator =  '/';

#ifdef  __cplusplus
extern "C" {
#endif
	


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
  Don't allow use of min() or max() macros
   - in order to enforce forward compatibility
*/

#ifdef min
#undef min
#endif

#ifdef max
#undef max
#endif

#define NDB_O_DIRECT_WRITE_ALIGNMENT 512
#define NDB_O_DIRECT_WRITE_BLOCKSIZE 4096

#define NDB_ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))

#if defined(_WIN32) && (_MSC_VER > 1500)
#define HAVE___HAS_TRIVIAL_CONSTRUCTOR
#define HAVE___IS_POD
#endif

/**
 * visual studio is stricter than gcc for __is_pod, settle for __has_trivial_constructor
 *  until we really really made all signal data classes POD
 *
 * UPDATE: also gcc fails to compile our code with gcc4.4.3
 */
#ifdef HAVE___HAS_TRIVIAL_CONSTRUCTOR
#define NDB_ASSERT_POD(x) \
  static_assert(__has_trivial_constructor(x))
#else
#define NDB_ASSERT_POD(x)
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

struct LinearSectionPtr
{
  Uint32 sz;
  const Uint32* p;
};

struct SegmentedSectionPtrPOD
{
  Uint32 sz;
  Uint32 i;
  struct SectionSegment * p;

#ifdef __cplusplus
  void setNull() { p = nullptr;}
  bool isNull() const { return p == nullptr;}
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

  void setNull() { p = nullptr;}
  bool isNull() const { return p == nullptr;}
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
  virtual ~GenericSectionIterator() {}
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
