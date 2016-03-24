/*
   Copyright (c) 2001, 2016, Oracle and/or its affiliates. All rights reserved.

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

#ifndef MY_GLOBAL_INCLUDED
#define MY_GLOBAL_INCLUDED

/**
  @file include/my_global.h
  This include file should be included first in every header file.

  This makes sure my_config.h is included to get platform specific
  symbols defined and it makes sure a lot of platform/compiler
  differences are mitigated.
*/

#include "my_config.h"

#define __STDC_LIMIT_MACROS	/* Enable C99 limit macros */
#define __STDC_FORMAT_MACROS	/* Enable C99 printf format macros */
#define _USE_MATH_DEFINES       /* Get access to M_PI, M_E, etc. in math.h */

#ifdef _WIN32
/* Include common headers.*/
# include <winsock2.h>
# include <ws2tcpip.h> /* SOCKET */
# include <io.h>       /* access(), chmod() */
#endif

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stddef.h>
#include <math.h>
#include <limits.h>
#include <float.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>				/* Recommended by debian */
#include <sys/types.h>

#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#if !defined(_WIN32)
#include <netdb.h>
#endif
#ifdef MY_MSCRT_DEBUG
#include <crtdbg.h>
#endif

/*
  A lot of our programs uses asserts, so better to always include it
  This also fixes a problem when people uses DBUG_ASSERT without including
  assert.h
*/
#include <assert.h>

/* Include standard definitions of operator new and delete. */
#ifdef __cplusplus
# include <new>
#endif

#include "my_compiler.h"
#include "my_dbug.h"

/* Macros to make switching between C and C++ mode easier */
#ifdef __cplusplus
#define C_MODE_START    extern "C" {
#define C_MODE_END	}
#else
#define C_MODE_START
#define C_MODE_END
#endif

#ifdef WITH_PERFSCHEMA_STORAGE_ENGINE
#ifdef EMBEDDED_LIBRARY

#ifndef DISABLE_PSI_THREAD
#define DISABLE_PSI_THREAD
#endif

#ifndef DISABLE_PSI_MUTEX
#define DISABLE_PSI_MUTEX
#endif

#ifndef DISABLE_PSI_RWLOCK
#define DISABLE_PSI_RWLOCK
#endif

#ifndef DISABLE_PSI_COND
#define DISABLE_PSI_COND
#endif

#ifndef DISABLE_PSI_FILE
#define DISABLE_PSI_FILE
#endif

#ifndef DISABLE_PSI_TABLE
#define DISABLE_PSI_TABLE
#endif

#ifndef DISABLE_PSI_SOCKET
#define DISABLE_PSI_SOCKET
#endif

#ifndef DISABLE_PSI_STAGE
#define DISABLE_PSI_STAGE
#endif

#ifndef DISABLE_PSI_STATEMENT
#define DISABLE_PSI_STATEMENT
#endif

#ifndef DISABLE_PSI_SP
#define DISABLE_PSI_SP
#endif

#ifndef DISABLE_PSI_PS
#define DISABLE_PSI_PS
#endif

#ifndef DISABLE_PSI_IDLE
#define DISABLE_PSI_IDLE
#endif

#ifndef DISABLE_PSI_STATEMENT_DIGEST
#define DISABLE_PSI_STATEMENT_DIGEST
#endif

#ifndef DISABLE_PSI_METADATA
#define DISABLE_PSI_METADATA
#endif

#ifndef DISABLE_PSI_MEMORY
#define DISABLE_PSI_MEMORY
#endif

#ifndef DISABLE_PSI_TRANSACTION
#define DISABLE_PSI_TRANSACTION
#endif

#endif /* EMBEDDED_LIBRARY */
#endif /* WITH_PERFSCHEMA_STORAGE_ENGINE */

#ifdef WITH_PERFSCHEMA_STORAGE_ENGINE
#define HAVE_PSI_INTERFACE
#endif /* WITH_PERFSCHEMA_STORAGE_ENGINE */

#ifdef HAVE_PSI_INTERFACE

 /**
  @def DISABLE_PSI_MUTEX
  Compiling option to disable the mutex instrumentation.
  This option is mostly intended to be used during development,
  when doing special builds with only a subset of the performance schema instrumentation,
  for code analysis / profiling / performance tuning of a specific instrumentation alone.
  @sa DISABLE_PSI_RWLOCK
  @sa DISABLE_PSI_COND
  @sa DISABLE_PSI_FILE
  @sa DISABLE_PSI_THREAD
  @sa DISABLE_PSI_TABLE
  @sa DISABLE_PSI_STAGE
  @sa DISABLE_PSI_STATEMENT
  @sa DISABLE_PSI_SP
  @sa DISABLE_PSI_PS
  @sa DISABLE_PSI_STATEMENT_DIGEST
  @sa DISABLE_PSI_SOCKET
  @sa DISABLE_PSI_MEMORY
  @sa DISABLE_PSI_IDLE
  @sa DISABLE_PSI_METADATA
  @sa DISABLE_PSI_TRANSACTION
*/

#ifndef DISABLE_PSI_MUTEX
#define HAVE_PSI_MUTEX_INTERFACE
#endif /* DISABLE_PSI_MUTEX */

/**
  @def DISABLE_PSI_RWLOCK
  Compiling option to disable the rwlock instrumentation.
  @sa DISABLE_PSI_MUTEX
*/

#ifndef DISABLE_PSI_RWLOCK
#define HAVE_PSI_RWLOCK_INTERFACE
#endif /* DISABLE_PSI_RWLOCK */

/**
  @def DISABLE_PSI_COND
  Compiling option to disable the cond instrumentation.
  @sa DISABLE_PSI_MUTEX
*/

#ifndef DISABLE_PSI_COND
#define HAVE_PSI_COND_INTERFACE
#endif /* DISABLE_PSI_COND */

/**
  @def DISABLE_PSI_FILE
  Compiling option to disable the file instrumentation.
  @sa DISABLE_PSI_MUTEX
*/

#ifndef DISABLE_PSI_FILE
#define HAVE_PSI_FILE_INTERFACE
#endif /* DISABLE_PSI_FILE */

/**
  @def DISABLE_PSI_THREAD
  Compiling option to disable the thread instrumentation.
  @sa DISABLE_PSI_MUTEX
*/

#ifndef DISABLE_PSI_THREAD
#define HAVE_PSI_THREAD_INTERFACE
#endif /* DISABLE_PSI_THREAD */

/**
  @def DISABLE_PSI_TABLE
  Compiling option to disable the table instrumentation.
  @sa DISABLE_PSI_MUTEX
*/

#ifndef DISABLE_PSI_TABLE
#define HAVE_PSI_TABLE_INTERFACE
#endif /* DISABLE_PSI_TABLE */

/**
  @def DISABLE_PSI_STAGE
  Compiling option to disable the stage instrumentation.
  @sa DISABLE_PSI_MUTEX
*/

#ifndef DISABLE_PSI_STAGE
#define HAVE_PSI_STAGE_INTERFACE
#endif /* DISABLE_PSI_STAGE */

/**
  @def DISABLE_PSI_STATEMENT
  Compiling option to disable the statement instrumentation.
  @sa DISABLE_PSI_MUTEX
*/

#ifndef DISABLE_PSI_STATEMENT
#define HAVE_PSI_STATEMENT_INTERFACE
#endif /* DISABLE_PSI_STATEMENT */

/**
  @def DISABLE_PSI_SP
  Compiling option to disable the stored program instrumentation.
  @sa DISABLE_PSI_MUTEX
*/

#ifndef DISABLE_PSI_SP
#define HAVE_PSI_SP_INTERFACE
#endif /* DISABLE_PSI_SP */

/**
  @def DISABLE_PSI_PS
  Compiling option to disable the prepared statement instrumentation.
  @sa DISABLE_PSI_MUTEX
*/

#ifndef DISABLE_PSI_STATEMENT
#ifndef DISABLE_PSI_PS
#define HAVE_PSI_PS_INTERFACE
#endif /* DISABLE_PSI_PS */
#endif /* DISABLE_PSI_STATEMENT */

/**
  @def DISABLE_PSI_STATEMENT_DIGEST
  Compiling option to disable the statement digest instrumentation.
*/

#ifndef DISABLE_PSI_STATEMENT
#ifndef DISABLE_PSI_STATEMENT_DIGEST
#define HAVE_PSI_STATEMENT_DIGEST_INTERFACE
#endif /* DISABLE_PSI_STATEMENT_DIGEST */
#endif /* DISABLE_PSI_STATEMENT */

/**
  @def DISABLE_PSI_TRANSACTION
  Compiling option to disable the transaction instrumentation.
  @sa DISABLE_PSI_MUTEX
*/

#ifndef DISABLE_PSI_TRANSACTION
#define HAVE_PSI_TRANSACTION_INTERFACE
#endif /* DISABLE_PSI_TRANSACTION */

/**
  @def DISABLE_PSI_SOCKET
  Compiling option to disable the statement instrumentation.
  @sa DISABLE_PSI_MUTEX
*/

#ifndef DISABLE_PSI_SOCKET
#define HAVE_PSI_SOCKET_INTERFACE
#endif /* DISABLE_PSI_SOCKET */

/**
  @def DISABLE_PSI_MEMORY
  Compiling option to disable the memory instrumentation.
  @sa DISABLE_PSI_MUTEX
*/

#ifndef DISABLE_PSI_MEMORY
#define HAVE_PSI_MEMORY_INTERFACE
#endif /* DISABLE_PSI_MEMORY */

/**
  @def DISABLE_PSI_IDLE
  Compiling option to disable the idle instrumentation.
  @sa DISABLE_PSI_MUTEX
*/

#ifndef DISABLE_PSI_IDLE
#define HAVE_PSI_IDLE_INTERFACE
#endif /* DISABLE_PSI_IDLE */

/**
  @def DISABLE_PSI_METADATA
  Compiling option to disable the metadata instrumentation.
  @sa DISABLE_PSI_MUTEX
*/

#ifndef DISABLE_PSI_METADATA
#define HAVE_PSI_METADATA_INTERFACE
#endif /* DISABLE_PSI_METADATA */

#endif /* HAVE_PSI_INTERFACE */

/* Make it easier to add conditional code in _expressions_ */
#ifdef _WIN32
#define IF_WIN(A,B) A
#else
#define IF_WIN(A,B) B
#endif

#if defined (_WIN32)
/*
 off_t is 32 bit long. We do not use C runtime functions
 with off_t but native Win32 file IO APIs, that work with
 64 bit offsets.
*/
#undef SIZEOF_OFF_T
#define SIZEOF_OFF_T 8

static inline void sleep(unsigned long seconds)
{
  Sleep(seconds * 1000);
}

/* Define missing access() modes. */
#define F_OK 0
#define W_OK 2
#define R_OK 4                        /* Test for read permission.  */

/* Define missing file locking constants. */
#define F_RDLCK 1
#define F_WRLCK 2
#define F_UNLCK 3
#define F_TO_EOF 0x3FFFFFFF

#define O_NONBLOCK 1    /* For emulation of fcntl() */

/*
  SHUT_RDWR is called SD_BOTH in windows and
  is defined to 2 in winsock2.h
  #define SD_BOTH 0x02
*/
#define SHUT_RDWR 0x02

/* Shared memory and named pipe connections are supported. */
#define shared_memory_buffer_length 16000
#define default_shared_memory_base_name "MYSQL"
#endif /* _WIN32*/

/* an assert that works at compile-time. only for constant expression */
#define compile_time_assert(X)                                              \
  do                                                                        \
  {                                                                         \
    typedef char compile_time_assert[(X) ? 1 : -1] MY_ATTRIBUTE((unused)); \
  } while(0)

/*
  Two levels of macros are needed to stringify the
  result of expansion of a macro argument.
*/
#define QUOTE_ARG(x)		#x	/* Quote argument (before cpp) */
#define STRINGIFY_ARG(x) QUOTE_ARG(x)	/* Quote argument, after cpp */

#if !defined(HAVE_UINT)
typedef unsigned int uint;
typedef unsigned short ushort;
#endif

#define MY_TEST(a)		((a) ? 1 : 0)
#define MY_MAX(a, b)	((a) > (b) ? (a) : (b))
#define MY_MIN(a, b)	((a) < (b) ? (a) : (b))
#define set_if_bigger(a,b)  do { if ((a) < (b)) (a)=(b); } while(0)
#define set_if_smaller(a,b) do { if ((a) > (b)) (a)=(b); } while(0)
#define test_all_bits(a,b) (((a) & (b)) == (b))
#define array_elements(A) ((uint) (sizeof(A)/sizeof(A[0])))

/* Define some general constants */
#ifndef TRUE
#define TRUE		(1)	/* Logical true */
#define FALSE		(0)	/* Logical false */
#endif

/* Some types that is different between systems */

typedef int	File;		/* File descriptor */
#ifdef _WIN32
typedef SOCKET my_socket;
#else
typedef int	my_socket;	/* File descriptor for sockets */
#define INVALID_SOCKET -1
#endif
#if defined(__GNUC__)
typedef char	pchar;		/* Mixed prototypes can take char */
typedef char	pbool;		/* Mixed prototypes can take char */
#else
typedef int	pchar;		/* Mixed prototypes can't take char */
typedef int	pbool;		/* Mixed prototypes can't take char */
#endif
#ifdef _WIN32
typedef int       socket_len_t;
typedef int       sigset_t;
typedef int       mode_t;
typedef SSIZE_T   ssize_t;
#else
typedef socklen_t socket_len_t;
#endif

/* file create flags */

#ifndef O_SHARE			/* Probably not windows */
#define O_SHARE		0	/* Flag to my_open for shared files */
#ifndef O_BINARY
#define O_BINARY	0	/* Flag to my_open for binary files */
#endif
#ifndef FILE_BINARY
#define FILE_BINARY	O_BINARY /* Flag to my_fopen for binary streams */
#endif
#ifdef HAVE_FCNTL
#define F_TO_EOF	0L	/* Param to lockf() to lock rest of file */
#endif
#endif /* O_SHARE */

#ifndef O_TEMPORARY
#define O_TEMPORARY	0
#endif
#ifndef O_SHORT_LIVED
#define O_SHORT_LIVED	0
#endif
#ifndef O_NOFOLLOW
#define O_NOFOLLOW      0
#endif

/* additional file share flags for win32 */
#ifdef _WIN32
#define _SH_DENYRWD     0x110    /* deny read/write mode & delete */
#define _SH_DENYWRD     0x120    /* deny write mode & delete      */
#define _SH_DENYRDD     0x130    /* deny read mode & delete       */
#define _SH_DENYDEL     0x140    /* deny delete only              */
#endif /* _WIN32 */


/* General constants */
#define FN_LEN		256	/* Max file name len */
#define FN_HEADLEN	253	/* Max length of filepart of file name */
#define FN_EXTLEN	20	/* Max length of extension (part of FN_LEN) */
#define FN_REFLEN	512	/* Max length of full path-name */
#define FN_REFLEN_SE	4000	/* Max length of full path-name in SE */
#define FN_EXTCHAR	'.'
#define FN_HOMELIB	'~'	/* ~/ is used as abbrev for home dir */
#define FN_CURLIB	'.'	/* ./ is used as abbrev for current dir */
#define FN_PARENTDIR	".."	/* Parent directory; Must be a string */

#ifdef _WIN32
#define FN_LIBCHAR	'\\'
#define FN_LIBCHAR2	'/'
#define FN_DIRSEP       "/\\"               /* Valid directory separators */
#define FN_EXEEXT   ".exe"
#define FN_SOEXT    ".dll"
#define FN_ROOTDIR	"\\"
#define FN_DEVCHAR	':'
#define FN_NETWORK_DRIVES	/* Uses \\ to indicate network drives */
#else
#define FN_LIBCHAR	'/'
#define FN_LIBCHAR2	'/'
#define FN_DIRSEP       "/"     /* Valid directory separators */
#define FN_EXEEXT   ""
#define FN_SOEXT    ".so"
#define FN_ROOTDIR	"/"
#endif

/* 
  MY_FILE_MIN is  Windows speciality and is used to quickly detect
  the mismatch of CRT and mysys file IO usage on Windows at runtime.
  CRT file descriptors can be in the range 0-2047, whereas descriptors returned
  by my_open() will start with 2048. If a file descriptor with value less then
  MY_FILE_MIN is passed to mysys IO function, chances are it stemms from
  open()/fileno() and not my_open()/my_fileno.

  For Posix,  mysys functions are light wrappers around libc, and MY_FILE_MIN
  is logically 0.
*/

#ifdef _WIN32
#define MY_FILE_MIN  2048
#else
#define MY_FILE_MIN  0
#endif

/* 
  MY_NFILE is the default size of my_file_info array.

  It is larger on Windows, because it all file handles are stored in my_file_info
  Default size is 16384 and this should be enough for most cases.If it is not 
  enough, --max-open-files with larger value can be used.

  For Posix , my_file_info array is only used to store filenames for
  error reporting and its size is not a limitation for number of open files.
*/ 
#ifdef _WIN32
#define MY_NFILE (16384 + MY_FILE_MIN)
#else
#define MY_NFILE 64
#endif

#define OS_FILE_LIMIT	UINT_MAX

/*
  Io buffer size; Must be a power of 2 and a multiple of 512. May be
  smaller what the disk page size. This influences the speed of the
  isam btree library. eg to big to slow.
*/
#define IO_SIZE			4096
/*
  How much overhead does malloc have. The code often allocates
  something like 1024-MALLOC_OVERHEAD bytes
*/
#define MALLOC_OVERHEAD 8

	/* get memory in huncs */
#define ONCE_ALLOC_INIT		(uint) (4096-MALLOC_OVERHEAD)
	/* Typical record cash */
#define RECORD_CACHE_SIZE	(uint) (64*1024-MALLOC_OVERHEAD)


/* Some defines of functions for portability */

#ifdef _WIN32
#if !defined(_WIN64)
inline double my_ulonglong2double(unsigned long long value)
{
  long long nr=(long long) value;
  if (nr >= 0)
    return (double) nr;
  return (18446744073709551616.0 + (double) nr);
}
#define ulonglong2double my_ulonglong2double
#define my_off_t2double  my_ulonglong2double
#endif /* _WIN64 */
inline unsigned long long my_double2ulonglong(double d)
{
  double t= d - (double) 0x8000000000000000ULL;

  if (t >= 0)
    return  ((unsigned long long) t) + 0x8000000000000000ULL;
  return (unsigned long long) d;
}
#define double2ulonglong my_double2ulonglong
#endif /* _WIN32 */

#ifndef ulonglong2double
#define ulonglong2double(A) ((double) (ulonglong) (A))
#define my_off_t2double(A)  ((double) (my_off_t) (A))
#endif
#ifndef double2ulonglong
#define double2ulonglong(A) ((ulonglong) (double) (A))
#endif

#define INT_MIN64       (~0x7FFFFFFFFFFFFFFFLL)
#define INT_MAX64       0x7FFFFFFFFFFFFFFFLL
#define INT_MIN32       (~0x7FFFFFFFL)
#define INT_MAX32       0x7FFFFFFFL
#define UINT_MAX32      0xFFFFFFFFL
#define INT_MIN24       (~0x007FFFFF)
#define INT_MAX24       0x007FFFFF
#define UINT_MAX24      0x00FFFFFF
#define INT_MIN16       (~0x7FFF)
#define INT_MAX16       0x7FFF
#define UINT_MAX16      0xFFFF
#define INT_MIN8        (~0x7F)
#define INT_MAX8        0x7F
#define UINT_MAX8       0xFF

#ifndef SIZE_T_MAX
#define SIZE_T_MAX      (~((size_t) 0))
#endif

#if defined(__cplusplus)
  /*
    For C++ use the new C++11 std functions rather than C99 macros.
    For C use C99 macros - see storage/myisam/rt_split.c
  */
  #define my_isfinite please_use_std_isfinite_rather_than_my_isfinite
  #define my_isnan(X) please_use_std_isnan_rather_than_my_isnan
  #define my_isinf(X) please_use_std_isinf_rather_than_my_isinf
#endif /* __cplusplus */

/*
  Max size that must be added to a so that we know Size to make
  adressable obj.
*/
#if SIZEOF_CHARP == 4
typedef long		my_ptrdiff_t;
#else
typedef long long	my_ptrdiff_t;
#endif

#define MY_ALIGN(A,L)	(((A) + (L) - 1) & ~((L) - 1))
#define ALIGN_SIZE(A)	MY_ALIGN((A),sizeof(double))
/* Size to make adressable obj. */
#define ADD_TO_PTR(ptr,size,type) (type) ((uchar*) (ptr)+size)
#define PTR_BYTE_DIFF(A,B) (my_ptrdiff_t) ((uchar*) (A) - (uchar*) (B))

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

#define NullS		(char *) 0

#ifdef _WIN32
#define STDCALL __stdcall
#else
#define STDCALL
#endif

/* Typdefs for easyier portability */

typedef unsigned char	uchar;	/* Short for unsigned char */
typedef signed char int8;       /* Signed integer >= 8  bits */
typedef unsigned char uint8;    /* Unsigned integer >= 8  bits */
typedef short int16;
typedef unsigned short uint16;
#if SIZEOF_INT == 4
typedef int int32;
typedef unsigned int uint32;
#elif SIZEOF_LONG == 4
typedef long int32;
typedef unsigned long uint32;
#else
#error Neither int or long is of 4 bytes width
#endif

#if !defined(HAVE_ULONG)
typedef unsigned long	ulong;		  /* Short for unsigned long */
#endif
/* 
  Using [unsigned] long long is preferable as [u]longlong because we use 
  [unsigned] long long unconditionally in many places, 
  for example in constants with [U]LL suffix.
*/
typedef unsigned long long int ulonglong; /* ulong or unsigned long long */
typedef long long int	longlong;
typedef longlong int64;
typedef ulonglong uint64;

#if defined (_WIN32)
typedef unsigned __int64 my_ulonglong;
#else
typedef unsigned long long my_ulonglong;
#endif

#if SIZEOF_CHARP == SIZEOF_INT
typedef int intptr;
#elif SIZEOF_CHARP == SIZEOF_LONG
typedef long intptr;
#elif SIZEOF_CHARP == SIZEOF_LONG_LONG
typedef long long intptr;
#else
#error sizeof(void *) is neither sizeof(int) nor sizeof(long) nor sizeof(long long)
#endif

#if defined(_WIN32)
typedef unsigned long long my_off_t;
#else
#if SIZEOF_OFF_T > 4
typedef ulonglong my_off_t;
#else
typedef unsigned long my_off_t;
#endif
#endif /*_WIN32*/
#define MY_FILEPOS_ERROR	(~(my_off_t) 0)

/*
  TODO Convert these to use Bitmap class.
 */
typedef ulonglong table_map;          /* Used for table bits in join */
typedef ulonglong nesting_map;  /* Used for flags of nesting constructs */

#if defined(_WIN32)
#define socket_errno	WSAGetLastError()
#define SOCKET_EINTR	WSAEINTR
#define SOCKET_EAGAIN	WSAEINPROGRESS
#define SOCKET_EWOULDBLOCK WSAEWOULDBLOCK
#define SOCKET_EADDRINUSE WSAEADDRINUSE
#define SOCKET_ETIMEDOUT WSAETIMEDOUT
#define SOCKET_ECONNRESET WSAECONNRESET
#define SOCKET_ENFILE	ENFILE
#define SOCKET_EMFILE	EMFILE
#else /* Unix */
#define socket_errno	errno
#define closesocket(A)	close(A)
#define SOCKET_EINTR	EINTR
#define SOCKET_EAGAIN	EAGAIN
#define SOCKET_EWOULDBLOCK EWOULDBLOCK
#define SOCKET_EADDRINUSE EADDRINUSE
#define SOCKET_ETIMEDOUT ETIMEDOUT
#define SOCKET_ECONNRESET ECONNRESET
#define SOCKET_ENFILE	ENFILE
#define SOCKET_EMFILE	EMFILE
#endif

typedef int		myf;	/* Type of MyFlags in my_funcs */
typedef char		my_bool; /* Small bool */

#if !defined(__cplusplus) && !defined(bool)
#define bool In_C_you_should_use_my_bool_instead()
#endif

/* Macros for converting *constants* to the right type */
#define MYF(v)		(myf) (v)

#if defined(_WIN32)
#define dlsym(lib, name) (void*)GetProcAddress((HMODULE)lib, name)
#define dlopen(libname, unused) LoadLibraryEx(libname, NULL, 0)
#define dlclose(lib) FreeLibrary((HMODULE)lib)
#define DLERROR_GENERATE(errmsg, error_number) \
  char win_errormsg[2048]; \
  if(FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, \
                   0, error_number, 0, win_errormsg, 2048, NULL)) \
  { \
    char *ptr; \
    for (ptr= &win_errormsg[0] + strlen(win_errormsg) - 1; \
         ptr >= &win_errormsg[0] && strchr("\r\n\t\0x20", *ptr); \
         ptr--) \
      *ptr= 0; \
    errmsg= win_errormsg; \
  } \
  else \
    errmsg= ""
#define dlerror() ""
#define dlopen_errno GetLastError()

#else /* _WIN32 */
#define DLERROR_GENERATE(errmsg, error_number) errmsg= dlerror()
#define dlopen_errno errno
#endif /* _WIN32 */

/* Length of decimal number represented by INT32. */
#define MY_INT32_NUM_DECIMAL_DIGITS 11U

/* Length of decimal number represented by INT64. */
#define MY_INT64_NUM_DECIMAL_DIGITS 21U

/* 
  MYSQL_PLUGIN_IMPORT macro is used to export mysqld data
  (i.e variables) for usage in storage engine loadable plugins.
  Outside of Windows, it is dummy.
*/
#if (defined(_WIN32) && defined(MYSQL_DYNAMIC_PLUGIN))
#define MYSQL_PLUGIN_IMPORT __declspec(dllimport)
#else
#define MYSQL_PLUGIN_IMPORT
#endif

#ifdef EMBEDDED_LIBRARY
#define NO_EMBEDDED_ACCESS_CHECKS
/* Things we don't need in the embedded version of MySQL */
#undef HAVE_OPENSSL
#endif /* EMBEDDED_LIBRARY */


enum loglevel {
   ERROR_LEVEL=       0,
   WARNING_LEVEL=     1,
   INFORMATION_LEVEL= 2
};


#ifdef _WIN32
/****************************************************************************
** Replacements for localtime_r and gmtime_r
****************************************************************************/

static inline struct tm *localtime_r(const time_t *timep, struct tm *tmp)
{
  localtime_s(tmp, timep);
  return tmp;
}

static inline struct tm *gmtime_r(const time_t *clock, struct tm *res)
{
  gmtime_s(res, clock);
  return res;
}
#endif /* _WIN32 */

C_MODE_START
ulonglong my_getsystime(void);

void set_timespec_nsec(struct timespec *abstime, ulonglong nsec);

void set_timespec(struct timespec *abstime, ulonglong sec);
C_MODE_END

/**
   Compare two timespec structs.

   @retval  1 If ts1 ends after ts2.
   @retval -1 If ts1 ends before ts2.
   @retval  0 If ts1 is equal to ts2.
*/
static inline int cmp_timespec(struct timespec *ts1, struct timespec *ts2)
{
  if (ts1->tv_sec > ts2->tv_sec ||
      (ts1->tv_sec == ts2->tv_sec && ts1->tv_nsec > ts2->tv_nsec))
    return 1;
  if (ts1->tv_sec < ts2->tv_sec ||
      (ts1->tv_sec == ts2->tv_sec && ts1->tv_nsec < ts2->tv_nsec))
    return -1;
  return 0;
}

static inline ulonglong diff_timespec(struct timespec *ts1, struct timespec *ts2)
{
  return (ts1->tv_sec - ts2->tv_sec) * 1000000000ULL +
    ts1->tv_nsec - ts2->tv_nsec;
}

#ifdef _WIN32
typedef int MY_MODE;
#else
typedef mode_t MY_MODE;
#endif /* _WIN32 */

/* File permissions */
#define USER_READ       (1L << 0)
#define USER_WRITE      (1L << 1)
#define USER_EXECUTE    (1L << 2)
#define GROUP_READ      (1L << 3)
#define GROUP_WRITE     (1L << 4)
#define GROUP_EXECUTE   (1L << 5)
#define OTHERS_READ     (1L << 6)
#define OTHERS_WRITE    (1L << 7)
#define OTHERS_EXECUTE  (1L << 8)
#define USER_RWX        USER_READ | USER_WRITE | USER_EXECUTE
#define GROUP_RWX       GROUP_READ | GROUP_WRITE | GROUP_EXECUTE
#define OTHERS_RWX      OTHERS_READ | OTHERS_WRITE | OTHERS_EXECUTE

/* Defaults */
#define DEFAULT_SSL_CA_CERT     "ca.pem"
#define DEFAULT_SSL_CA_KEY      "ca-key.pem"
#define DEFAULT_SSL_SERVER_CERT "server-cert.pem"
#define DEFAULT_SSL_SERVER_KEY  "server-key.pem"

#endif  // MY_GLOBAL_INCLUDED
