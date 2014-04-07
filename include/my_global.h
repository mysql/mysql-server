/*
   Copyright (c) 2001, 2014, Oracle and/or its affiliates. All rights reserved.

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

/* This is the include file that should be included 'first' in every C file. */

/*
  InnoDB depends on some MySQL internals which other plugins should not
  need.  This is because of InnoDB's foreign key support, "safe" binlog
  truncation, and other similar legacy features.

  We define accessors for these internals unconditionally, but do not
  expose them in mysql/plugin.h.  They are declared in ha_innodb.h for
  InnoDB's use.
*/
#define INNODB_COMPATIBILITY_HOOKS

#if defined(i386) && !defined(__i386__)
#define __i386__
#endif

/* Macros to make switching between C and C++ mode easier */
#ifdef __cplusplus
#define C_MODE_START    extern "C" {
#define C_MODE_END	}
#else
#define C_MODE_START
#define C_MODE_END
#endif

#ifdef __cplusplus
#define CPP_UNNAMED_NS_START  namespace {
#define CPP_UNNAMED_NS_END    }
#endif

#include <my_config.h>

#ifdef WITH_PERFSCHEMA_STORAGE_ENGINE
#define HAVE_PSI_INTERFACE
#endif /* WITH_PERFSCHEMA_STORAGE_ENGINE */

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

/*
 Prevent inclusion of  Windows GDI headers - they define symbol
 ERROR that conflicts with mysql headers.
*/
#ifndef NOGDI
#define NOGDI
#endif

/* Include common headers.*/
#include <winsock2.h>
#include <ws2tcpip.h> /* SOCKET */
#include <io.h>       /* access(), chmod() */
#include <process.h>  /* getpid() */

#define sleep(a) Sleep((a)*1000)

/* Define missing access() modes. */
#define F_OK 0
#define W_OK 2
#define R_OK 4                        /* Test for read permission.  */

/* Define missing file locking constants. */
#define F_RDLCK 1
#define F_WRLCK 2
#define F_UNLCK 3
#define F_TO_EOF 0x3FFFFFFF

/* Shared memory and named pipe connections are supported. */
#define shared_memory_buffer_length 16000
#define default_shared_memory_base_name "MYSQL"
#endif /* _WIN32*/

/*
  The macros below are borrowed from include/linux/compiler.h in the
  Linux kernel. Use them to indicate the likelyhood of the truthfulness
  of a condition. This serves two purposes - newer versions of gcc will be
  able to optimize for branch predication, which could yield siginficant
  performance gains in frequently executed sections of the code, and the
  other reason to use them is for documentation
*/
#ifdef HAVE_BUILTIN_EXPECT
#  define likely(x)    __builtin_expect((x),1)
#  define unlikely(x)  __builtin_expect((x),0)
#else
#  define likely(x)    (x)
#  define unlikely(x)  (x)
#endif

/* Fix problem with S_ISLNK() on Linux */
#if defined(TARGET_OS_LINUX) || defined(__GLIBC__)
#undef  _GNU_SOURCE
#define _GNU_SOURCE 1
#endif

/*
  Temporary solution to solve bug#7156. Include "sys/types.h" before
  the thread headers, else the function madvise() will not be defined
*/
#if defined(HAVE_SYS_TYPES_H) && ( defined(sun) || defined(__sun) )
#include <sys/types.h>
#endif

#define __EXTENSIONS__ 1	/* We want some extension */

/*
  Solaris 9 include file <sys/feature_tests.h> refers to X/Open document

    System Interfaces and Headers, Issue 5

  saying we should define _XOPEN_SOURCE=500 to get POSIX.1c prototypes,
  but apparently other systems (namely FreeBSD) don't agree.

  On a newer Solaris 10, the above file recognizes also _XOPEN_SOURCE=600.
  Furthermore, it tests that if a program requires older standard
  (_XOPEN_SOURCE<600 or _POSIX_C_SOURCE<200112L) it cannot be
  run on a new compiler (that defines _STDC_C99) and issues an #error.
  It's also an #error if a program requires new standard (_XOPEN_SOURCE=600
  or _POSIX_C_SOURCE=200112L) and a compiler does not define _STDC_C99.

  To add more to this mess, Sun Studio C compiler defines _STDC_C99 while
  C++ compiler does not!

  So, in a desperate attempt to get correct prototypes for both
  C and C++ code, we define either _XOPEN_SOURCE=600 or _XOPEN_SOURCE=500
  depending on the compiler's announced C standard support.

  Cleaner solutions are welcome.
*/
#ifdef __sun
#if __STDC_VERSION__ - 0 >= 199901L
#define _XOPEN_SOURCE 600
#else
#define _XOPEN_SOURCE 500
#endif
#endif

#if !defined(_WIN32)
#ifndef _POSIX_PTHREAD_SEMANTICS
#define _POSIX_PTHREAD_SEMANTICS /* We want posix threads */
#endif

#define _REENTRANT	1	/* Some thread libraries require this */

#if !defined(_THREAD_SAFE)
#define _THREAD_SAFE            /* Required for OSF1 */
#endif
#include <pthread.h>
#endif /* !defined(_WIN32) */

#if defined(_lint) && !defined(lint)
#define lint
#endif

#ifndef stdin
#include <stdio.h>
#endif
#include <stdarg.h>
#include <stdlib.h>
#include <stddef.h>

#include <math.h>
#include <limits.h>
#include <float.h>
#ifdef HAVE_FENV_H
#include <fenv.h> /* For fesetround() */
#endif

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif /* TIME_WITH_SYS_TIME */
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_ALLOCA_H
#include <alloca.h>
#endif

#include <errno.h>				/* Recommended by debian */
/* We need the following to go around a problem with openssl on solaris */
#if defined(HAVE_CRYPT_H)
#include <crypt.h>
#endif

/**
  Cast a member of a structure to the structure that contains it.

  @param  ptr     Pointer to the member.
  @param  type    Type of the structure that contains the member.
  @param  member  Name of the member within the structure.
*/
#define my_container_of(ptr, type, member)              \
  ((type *)((char *)ptr - offsetof(type, member)))

/*
  A lot of our programs uses asserts, so better to always include it
  This also fixes a problem when people uses DBUG_ASSERT without including
  assert.h
*/
#include <assert.h>

/* an assert that works at compile-time. only for constant expression */
#define compile_time_assert(X)                                              \
  do                                                                        \
  {                                                                         \
    typedef char compile_time_assert[(X) ? 1 : -1] __attribute__((unused)); \
  } while(0)

/* Declare madvise where it is not declared for C++, like Solaris */
#if HAVE_MADVISE && !HAVE_DECL_MADVISE && defined(__cplusplus)
extern "C" int madvise(void *addr, size_t len, int behav);
#endif

#define QUOTE_ARG(x)		#x	/* Quote argument (before cpp) */
#define STRINGIFY_ARG(x) QUOTE_ARG(x)	/* Quote argument, after cpp */

#ifndef SO_EXT
#ifdef _WIN32
#define SO_EXT ".dll"
#elif defined(__APPLE__)
#define SO_EXT ".dylib"
#else
#define SO_EXT ".so"
#endif
#endif

#if !defined(HAVE_UINT)
#undef HAVE_UINT
#define HAVE_UINT
typedef unsigned int uint;
typedef unsigned short ushort;
#endif

#define swap_variables(t, a, b) { t dummy; dummy= a; a= b; b= dummy; }
#define MY_TEST(a)		((a) ? 1 : 0)
#define set_if_bigger(a,b)  do { if ((a) < (b)) (a)=(b); } while(0)
#define set_if_smaller(a,b) do { if ((a) > (b)) (a)=(b); } while(0)
#define test_all_bits(a,b) (((a) & (b)) == (b))
#define array_elements(A) ((uint) (sizeof(A)/sizeof(A[0])))

/* Define some general constants */
#ifndef TRUE
#define TRUE		(1)	/* Logical true */
#define FALSE		(0)	/* Logical false */
#endif

#include <my_compiler.h>

/* The DBUG_ON flag always takes precedence over default DBUG_OFF */
#if defined(DBUG_ON) && defined(DBUG_OFF)
#undef DBUG_OFF
#endif

/* Some types that is different between systems */

typedef int	File;		/* File descriptor */
#ifdef _WIN32
typedef SOCKET my_socket;
#else
typedef int	my_socket;	/* File descriptor for sockets */
#define INVALID_SOCKET -1
#endif
C_MODE_START
typedef void	(*sig_return)();/* Returns type from signal */
C_MODE_END
#if defined(__GNUC__) && !defined(_lint)
typedef char	pchar;		/* Mixed prototypes can take char */
typedef char	pbool;		/* Mixed prototypes can take char */
#else
typedef int	pchar;		/* Mixed prototypes can't take char */
typedef int	pbool;		/* Mixed prototypes can't take char */
#endif
C_MODE_START
typedef int	(*qsort_cmp)(const void *,const void *);
typedef int	(*qsort_cmp2)(const void*, const void *,const void *);
C_MODE_END
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
typedef SOCKET_SIZE_TYPE size_socket;

#ifndef SOCKOPT_OPTLEN_TYPE
#define SOCKOPT_OPTLEN_TYPE size_socket
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
#define HAVE_FCNTL_LOCK
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
#define FN_NO_CASE_SENCE	/* Files are not case-sensitive */
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

#ifndef OS_FILE_LIMIT
#define OS_FILE_LIMIT	UINT_MAX
#endif

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
	/* Typical key cash */
#define KEY_CACHE_SIZE		(uint) (8*1024*1024)
	/* Default size of a key cache block  */
#define KEY_CACHE_BLOCK_SIZE	(uint) 1024


/* Some defines of functions for portability */

#ifndef _WIN32
#define closesocket(A)	close(A)
#endif

#if (_MSC_VER)
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
#endif

#ifndef ulonglong2double
#define ulonglong2double(A) ((double) (ulonglong) (A))
#define my_off_t2double(A)  ((double) (my_off_t) (A))
#endif
#ifndef double2ulonglong
#define double2ulonglong(A) ((ulonglong) (double) (A))
#endif

#define ulong_to_double(X) ((double) (ulong) (X))

#ifndef STACK_DIRECTION
#error "please add -DSTACK_DIRECTION=1 or -1 to your CPPFLAGS"
#endif

/* This is from the old m-machine.h file */

#if SIZEOF_LONG_LONG > 4
#define HAVE_LONG_LONG 1
#endif

/*
  Some pre-ANSI-C99 systems like AIX 5.1 and Linux/GCC 2.95 define
  ULONGLONG_MAX, LONGLONG_MIN, LONGLONG_MAX; we use them if they're defined.
*/

#if defined(HAVE_LONG_LONG) && !defined(LONGLONG_MIN)
#define LONGLONG_MIN	((long long) 0x8000000000000000LL)
#define LONGLONG_MAX	((long long) 0x7FFFFFFFFFFFFFFFLL)
#endif

#if defined(HAVE_LONG_LONG) && !defined(ULONGLONG_MAX)
/* First check for ANSI C99 definition: */
#ifdef ULLONG_MAX
#define ULONGLONG_MAX  ULLONG_MAX
#else
#define ULONGLONG_MAX ((unsigned long long)(~0ULL))
#endif
#endif /* defined (HAVE_LONG_LONG) && !defined(ULONGLONG_MAX)*/

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

/* From limits.h instead */
#ifndef DBL_MIN
#define DBL_MIN		4.94065645841246544e-324
#define FLT_MIN		((float)1.40129846432481707e-45)
#endif
#ifndef DBL_MAX
#define DBL_MAX		1.79769313486231470e+308
#define FLT_MAX		((float)3.40282346638528860e+38)
#endif
#ifndef SIZE_T_MAX
#define SIZE_T_MAX      (~((size_t) 0))
#endif

#include <math.h>

#if (__cplusplus >= 201103L)
  /* For C++11 use the new std functions rather than C99 macros. */
  #include <cmath>
  #define my_isfinite(X) std::isfinite(X)
  #define my_isnan(X) std::isnan(X)
  #define my_isinf(X) std::isinf(X)
#else
  #ifdef HAVE_LLVM_LIBCPP /* finite is deprecated in libc++ */
    #define my_isfinite(X) isfinite(X)
  #elif defined _WIN32
    #define my_isfinite(X) _finite(X)
  #else
    #define my_isfinite(X) finite(X)
  #endif
  #define my_isnan(X) isnan(X)
  #ifdef HAVE_ISINF
    /* System-provided isinf() is available and safe to use */
    #define my_isinf(X) isinf(X)
  #else /* !HAVE_ISINF */
    #define my_isinf(X) (!my_isfinite(X) && !my_isnan(X))
  #endif
#endif /* __cplusplus >= 201103L */

/* Define missing math constants. */
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#ifndef M_E
#define M_E 2.7182818284590452354
#endif
#ifndef M_LN2
#define M_LN2 0.69314718055994530942
#endif

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

#ifdef STDCALL
#undef STDCALL
#endif

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

#if !defined(HAVE_ULONG) && !defined(__USE_MISC)
typedef unsigned long	ulong;		  /* Short for unsigned long */
#endif
#ifndef longlong_defined
/* 
  Using [unsigned] long long is preferable as [u]longlong because we use 
  [unsigned] long long unconditionally in many places, 
  for example in constants with [U]LL suffix.
*/
#if defined(HAVE_LONG_LONG) && SIZEOF_LONG_LONG == 8
typedef unsigned long long int ulonglong; /* ulong or unsigned long long */
typedef long long int	longlong;
#else
typedef unsigned long	ulonglong;	  /* ulong or unsigned long long */
typedef long		longlong;
#endif
#endif
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

#define MY_ERRPTR ((void*)(intptr)1)

#if defined(_WIN32)
typedef unsigned long long my_off_t;
typedef unsigned long long os_off_t;
#else
typedef off_t os_off_t;
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

/* Macros for converting *constants* to the right type */
#define MYF(v)		(myf) (v)

#ifndef LL
#ifdef HAVE_LONG_LONG
#define LL(A) A ## LL
#else
#define LL(A) A ## L
#endif
#endif

#ifndef ULL
#ifdef HAVE_LONG_LONG
#define ULL(A) A ## ULL
#else
#define ULL(A) A ## UL
#endif
#endif


/* Some helper macros */
#define YESNO(X) ((X) ? "yes" : "no")

#define MY_HOW_OFTEN_TO_WRITE	1000	/* How often we want info on screen */

#include <my_byteorder.h>

#ifdef HAVE_CHARSET_utf8
#define MYSQL_UNIVERSAL_CLIENT_CHARSET "utf8"
#else
#define MYSQL_UNIVERSAL_CLIENT_CHARSET MYSQL_DEFAULT_CHARSET_NAME
#endif

#if defined(EMBEDDED_LIBRARY)
#define NO_EMBEDDED_ACCESS_CHECKS
#endif

#if defined(_WIN32)
#define dlsym(lib, name) (void*)GetProcAddress((HMODULE)lib, name)
#define dlopen(libname, unused) LoadLibraryEx(libname, NULL, 0)
#define dlclose(lib) FreeLibrary((HMODULE)lib)
#ifndef HAVE_DLOPEN
#define HAVE_DLOPEN
#endif
#endif

#ifdef HAVE_DLOPEN
#if defined(HAVE_DLFCN_H)
#include <dlfcn.h>
#endif
#endif

#ifndef HAVE_DLERROR
#ifdef _WIN32
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
#define dlerror() "No support for dynamic loading (static build?)"
#define DLERROR_GENERATE(errmsg, error_number) errmsg= dlerror()
#define dlopen_errno errno
#endif /* _WIN32 */
#else /* HAVE_DLERROR */
#define DLERROR_GENERATE(errmsg, error_number) errmsg= dlerror()
#define dlopen_errno errno
#endif /* HAVE_DLERROR */


/*
 *  Include standard definitions of operator new and delete.
 */
#ifdef __cplusplus
#include <new>
#endif

/* Length of decimal number represented by INT32. */
#define MY_INT32_NUM_DECIMAL_DIGITS 11U

/* Length of decimal number represented by INT64. */
#define MY_INT64_NUM_DECIMAL_DIGITS 21U

/* Define some useful general macros (should be done after all headers). */
#define MY_MAX(a, b)	((a) > (b) ? (a) : (b))
#define MY_MIN(a, b)	((a) < (b) ? (a) : (b))

/*
  Only Linux is known to need an explicit sync of the directory to make sure a
  file creation/deletion/renaming in(from,to) this directory durable.
*/
#ifdef TARGET_OS_LINUX
#define NEED_EXPLICIT_SYNC_DIR 1
#endif

#if !defined(__cplusplus) && !defined(bool)
#define bool In_C_you_should_use_my_bool_instead()
#endif

/* Provide __func__ macro definition for Visual Studio. */
#if defined(_MSC_VER)
#  define __func__ __FUNCTION__
#endif

#ifndef HAVE_RINT
/**
   All integers up to this number can be represented exactly as double precision
   values (DBL_MANT_DIG == 53 for IEEE 754 hardware).
*/
#define MAX_EXACT_INTEGER ((1LL << DBL_MANT_DIG) - 1)

/**
   rint(3) implementation for platforms that do not have it.
   Always rounds to the nearest integer with ties being rounded to the nearest
   even integer to mimic glibc's rint() behavior in the "round-to-nearest"
   FPU mode. Hardware-specific optimizations are possible (frndint on x86).
   Unlike this implementation, hardware will also honor the FPU rounding mode.
*/

static inline double rint(double x)
{
  double f, i;
  f = modf(x, &i);
  /*
    All doubles with absolute values > MAX_EXACT_INTEGER are even anyway,
    no need to check it.
  */
  if (x > 0.0)
    i += (double) ((f > 0.5) || (f == 0.5 &&
                                 i <= (double) MAX_EXACT_INTEGER &&
                                 (longlong) i % 2));
  else
    i -= (double) ((f < -0.5) || (f == -0.5 &&
                                  i >= (double) -MAX_EXACT_INTEGER &&
                                  (longlong) i % 2));
  return i;
}
#endif /* HAVE_RINT */

/* 
  MYSQL_PLUGIN_IMPORT macro is used to export mysqld data
  (i.e variables) for usage in storage engine loadable plugins.
  Outside of Windows, it is dummy.
*/
#ifndef MYSQL_PLUGIN_IMPORT
#if (defined(_WIN32) && defined(MYSQL_DYNAMIC_PLUGIN))
#define MYSQL_PLUGIN_IMPORT __declspec(dllimport)
#else
#define MYSQL_PLUGIN_IMPORT
#endif
#endif

#include <my_dbug.h>

/* Defines that are unique to the embedded version of MySQL */

#ifdef EMBEDDED_LIBRARY

/* Things we don't need in the embedded version of MySQL */
/* TODO HF add #undef HAVE_VIO if we don't want client in embedded library */

#undef HAVE_OPENSSL

#endif /* EMBEDDED_LIBRARY */


enum loglevel {
   ERROR_LEVEL=       0,
   WARNING_LEVEL=     1,
   INFORMATION_LEVEL= 2
};

#endif  // MY_GLOBAL_INCLUDED
