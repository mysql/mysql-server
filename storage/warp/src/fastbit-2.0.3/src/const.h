// File: $Id$
// Author: John Wu <John.Wu at acm.org>
//      Lawrence Berkeley National Laboratory
// Copyright (c) 2000-2016 the Regents of the University of California
#ifndef IBIS_CONST_H
#define IBIS_CONST_H
// Primary contact: John Wu <John.Wu at acm.org>
//
///@file
/// Defines common data types, constants and macros.  Used by all files in
/// the IBIS implementation of FastBit from the Scientific Data Management
/// Research Group of Lawrence Berkeley National Laboratory.
///

#if defined(DEBUG) && !defined(_DEBUG)
#  define _DEBUG DEBUG
#elif !defined(DEBUG) && defined(_DEBUG) && _DEBUG + 0 > 1
#  define DEBUG _DEBUG - 1
#endif
// gcc's stl header files needs this one to work correctly 
#ifndef _PTHREADS
#  define _PTHREADS
#endif
#ifndef _REENTRANT
#  define _REENTRANT
#endif
#ifndef _ISOC90_SOURCE
#  define _ISOC90_SOURCE
#endif
// machine related feature selection
#if defined(__SUNPRO_CC)
#  ifndef __EXTENSIONS__
#    define __EXTENSIONS__
#  endif
#elif defined(__KCC)
// the following combination appears to get the right functions
#  ifdef _POSIX_C_SOURCE
#    undef _POSIX_C_SOURCE
#  endif
#  ifndef _XOPEN_VERSION
#    define _XOPEN_VERSION 4
#  else
#    undef  _XOPEN_VERSION
#    define _XOPEN_VERSION 4
#  endif
#endif
//  #if defined(__unix__) && !defined(__USE_UNIX98)
//  #define __USE_UNIX98
//  #endif
#ifndef HAVE_STRUCT_TIMESPEC
#if defined(__CYGWIN__) || defined(__MINGW32__)
#  define HAVE_STRUCT_TIMESPEC
#endif
#endif
// // require every compiler to support mutable keyword
// #if __cplusplus >= 199711L
// #  define HAVE_MUTABLE 1
// #elif defined(__GNUC__)
// #  define HAVE_MUTABLE (__GNUC__>2 || (__GNUC__>1 && __GNUC_MINOR__-0>=95))
// #elif defined(__SUNPROC_CC)
// #  define HAVE_MUTABLE (__SUNPRO_CC >= 0x500)
// #elif defined(_MSC_VER)
// #  define HAVE_MUTABLE (_MSC_VER >= 1200)
// #elif defined(__PGI)
// #  define HAVE_MUTABLE 1
// #elif defined(__sgi)
// #  define HAVE_MUTABLE (_COMPILER_VERSION > 600)
// #else
// #  define HAVE_MUTABLE 0
// #endif

//
// common headers needed
#if defined(_WIN32) && defined(_MSC_VER) && defined(_DEBUG)
// Enable memory debugging on windows environments
#  define _CRTDBG_MAP_ALLOC
#  include <stdlib.h>
#  include <crtdbg.h>
#endif

#include <errno.h>	// errno
#include <string.h>	// strerr, strcasecmp, strcmp, memcpy, strlen
#include <pthread.h>	// mutex lock, rwlock, conditional variables
#if !defined(WITHOUT_FASTBIT_CONFIG_H) && !defined(__MINGW32__) && !defined(_MSC_VER)
#  include "fastbit-config.h"	// macros defined by the configure script
#  ifdef HAVE_SYS_TYPES_H
#    include <sys/types.h>	// timespec, etc
#  endif
#  ifdef HAVE_STDINT_H
#    include <stdint.h>
#  endif
#else
#  if defined(__unix__)||defined(__linux__)||defined(__APPLE__)||defined(__CYGWIN__)||defined(__FreeBSD__)
#    define HAVE_VPRINTF 1
#    define HAVE_DIRENT_H 1
#  endif
#  if !defined(_MSC_VER)
#    include <stdint.h>
#  endif
#endif

// section to handle errno in a multithread program
#if defined(__SUNPRO_CC)
#  if defined(_REENTRANT)
#    ifdef errno
#      undef errno
#    endif
#    define errno (*(::___errno()))
#  endif // defined(_REENTRANT)
//  #elif defined(_WIN32)
//  #  if defined(_MT) || defined(_DLL)
//       extern int * __cdecl _errno(void);
//  #    define errno (*_errno(void))
//  #  else /* ndef _MT && ndef _DLL */
//       extern int errno;
//  #  endif /* _MT || _DLL */
#endif /* errno */

// Compiler independent definitions:
#ifndef FASTBIT_STRING
#define FASTBIT_STRING "FastBit ibis"
#endif
//#define TIME_BUF_LEN 32
#ifndef MAX_LINE
#define MAX_LINE 2048
#endif
// #ifndef mmax
// #define mmax(x,y) ((x)>(y))?(x):(y)
// #endif

/// PREFERRED_BLOCK_SIZE is the parameter used to determine the logical page
/// size during some I/O intensive operations, such as nested loop join.
/// Many CPUs have 512KB cache, setting this value to 256K (262144) will
/// allow about two such 'logical' block to be in cache at the same time,
/// which should be good to things like nested loop join.
#ifndef PREFERRED_BLOCK_SIZE
#define PREFERRED_BLOCK_SIZE 1048576
//#define PREFERRED_BLOCK_SIZE 262144
#endif

//
// Compiler dependent definitions:
#if defined(_CRAY) | defined(__KCC)
#  define  __LIM_H_PARAM_
#  include <sys/param.h>
#  include <inttypes.h>	// int32_t, ...

#elif defined(__sun)
#  include <limits.h>	// PATH_MAX, OPEN_MAX
#  include <inttypes.h>	// int32_t, ...

// use rwlock_t to simulate pthread_rwlock_t
#  ifndef PTHREAD_RWLOCK_INITIALIZER
#    include <synch.h>  // rwlock
#    define pthread_rwlock_t rwlock_t
#    define pthread_rwlock_init(lk, attr) rwlock_init(lk, attr, 0)
#    define pthread_rwlock_destroy rwlock_destroy
#    define pthread_rwlock_rdlock rw_rdlock
#    define pthread_rwlock_wrlock rw_wrlock
#    define pthread_rwlock_tryrdlock rw_tryrdlock
#    define pthread_rwlock_trywrlock rw_trywrlock
#    define pthread_rwlock_unlock rw_unlock
#    define PTHREAD_RWLOCK_INITIALIZER DEFAULTRWLOCK;
#  endif

#elif defined(__unix__) || defined(__HOS_AIX__)
#  include <limits.h>	// PATH_MAX, OPEN_MAX
#  ifdef __CYGWIN__ // cygwin port of gcc compiler
//commented out 2005/04/12 #  define __INSIDE_CYGWIN__
#    include <cygwin/types.h>
#  endif

#elif defined(_WIN32)
// don't need too many things from Windows header files
#  define WIN32_LEAN_AND_MEAN 
// if WINVER is not define, pretend to be on windows vista
#  ifndef WINVER
#    ifdef _WIN32_WINNT
#      define WINVER _WIN32_WINNT
#    else
#      define WINVER 0x0600
#    endif
#  endif
#  include <limits.h>	// PATH_MAX, OPEN_MAX
#  include <windows.h>
#  include <direct.h>	// _mkdir
#  define mkdir(x,y) _mkdir(x)
#  define chmod _chmod

#  if defined(__MINGW__) || defined(__MINGW32__) || defined(__MINGW64__)
#    include <stdint.h>
#  else
     // MS windows has its own exact-width types, use them
#    ifndef int16_t
#      define int16_t __int16
#    endif
#    ifndef uint16_t
#      define uint16_t unsigned __int16
#    endif
#    ifndef int32_t
#      define int32_t __int32
#    endif
#    ifndef uint32_t
#      define uint32_t unsigned __int32
#    endif
#    ifndef int64_t
#      define int64_t __int64
#    endif
#    ifndef uint64_t
#      define uint64_t unsigned __int64
#    endif
#  endif

#elif defined(__APPLE__)
#  include <stdint.h>	// int32_t, ...
#  include <sys/syslimits.h>

#else
#  include <stdint.h>	// int32_t, ...
#  include <syslimits.h> // PATH_MAX

#endif

// a hack to check for exact-width data types -- according the Open Group's
// definition of stdint.h, when the exact-width integer types are defined,
// their corresponding MAX values are also defined with #define.  Since the
// types themselves may be typedefs, the corresponding INTx_MAX are more
// reliable checks.
#if !(defined(HAVE_STDINT_H) || defined(__unix__) || defined(_WIN32) || defined(__APPLE__) || defined(__x86_64__) || defined(_STDINT_H))
#  ifndef INT16_MAX
#    define int16_t short int
#    define INT16_MAX (32767)
#  endif
#  ifndef UINT16_MAX
#    define uint16_t unsigned short int
#    define UINT16_MAX (65535)
#  endif
#  ifndef INT32_MAX
#    define int32_t int
#    define INT32_MAX (2147483647)
#  endif
#  ifndef UINT32_MAX
#    define uint32_t unsigned int
#    define UINT32_MAX (4294967295UL)
#  endif
#  ifndef INT64_MAX
#    define int64_t long long int
#    define INT64_MAX (9223372036854775807LL)
#  endif
#  ifndef UINT64_MAX
#    define uint64_t unsigned long long int
#    define UINT64_MAX (18446744073709551615ULL)
#  endif
#endif

#ifndef PATH_MAX
#  define PATH_MAX 512
#endif

// things for MS Windows only
// FASTBIT_DIRSEP  == the directory name separator
// FASTBIT_CXX_DLLSPEC == export/import symbols to/from DLL library under windows system
#if defined(_WIN32) && defined(_MSC_VER)
#  define FASTBIT_DIRSEP '\\'
#else
#  define FASTBIT_DIRSEP '/'
#endif

#if defined(_WIN32) && (defined(_MSC_VER) || defined(__MINGW32__))
#  if defined(_USRDLL) || defined(CXX_USE_DLL)
#    if defined(DLL_EXPORT)
#      define FASTBIT_CXX_DLLSPEC __declspec(dllexport)
#    else
#      define FASTBIT_CXX_DLLSPEC __declspec(dllimport)
#    endif
#  else
#    define FASTBIT_CXX_DLLSPEC
#  endif
#else
#  define FASTBIT_CXX_DLLSPEC
#endif

/* causes problems on solaris 2.8
#ifdef PTHREAD_RWLOCK_INITIALIZER
#if xPTHREAD_RWLOCK_INITIALIZER == x
#undef PTHREAD_RWLOCK_INITIALIZER
#endif
#endif
*/
#if defined(__APPLE__) && !defined(PTHREAD_RWLOCK_INITIALIZER)
#define PTHREAD_RWLOCK_INITIALIZER
#endif
// still don't have correct RWLOCK, then use mutex lock instead
#ifndef PTHREAD_RWLOCK_INITIALIZER
#define IBIS_REPLACEMENT_RWLOCK
#define THREAD_RWLOCK_INITIALIZER \
		{PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER, \
		PTHREAD_COND_INITIALIZER, 0, 0}
typedef struct _rwlock {
    pthread_mutex_t lock;     // lock for structure          
    pthread_cond_t  readers;  // waiting readers             
    pthread_cond_t  writers;  // waiting writers            
    int             state;    // -1:writer,0:free,>0:readers 
    int             waiters;  // number of waiting writers  
} pthread_rwlock_t;
int pthread_rwlock_init(pthread_rwlock_t *rwlock, void*);
int pthread_rwlock_destroy(pthread_rwlock_t *rwlock);
int pthread_rwlock_rdlock(pthread_rwlock_t *rwlock);
int pthread_rwlock_wrlock(pthread_rwlock_t *rwlock);
int pthread_rwlock_tryrdlock(pthread_rwlock_t *rwlock);
int pthread_rwlock_trywrlock(pthread_rwlock_t *rwlock);
int pthread_rwlock_unlock(pthread_rwlock_t *rwlock);
#endif

/**
   A function prototype for delayed index reconstruction.  This function is
   used to read a portion of a 1-D array of 32-bit unsigned integers.  It
   is meant to read bitmaps while answering queries, where the bitmaps have
   been serialized and packed together using a concrete versin of
   ibis::index::write.

   @arg context: an opaque pointer used to stored the context information
   for the source array.
   @arg start: assume the source is a simple 1-D uint32_t array, this is the
   position (starting from 0) of the first element to be read.  This is
   also known as the offset.
   @arg count: the number of elements to be read.  Following
   C/C++ convention, this points to the elment just beyond the values to be
   read.
   @arg data: the pointer to the output buffer for holding the values read
   into memory.

   @return >= 0 to indicate success, < 0 to indicate error.

   @note This was previously named FastBitReadIntArray, which was meant to
   say "integer array."  Since the introduction of FastBitReadExtArray, the
   prefix "Int" could be misinterpreted as "internal" in contrast with
   "Ext" for "External."
*/
typedef int (*FastBitReadBitmaps)
(void *context, uint64_t start, uint64_t count, uint32_t *data);

/**
   A function prototype for reading a portion of an external array.  The
   user data is viwed as a numti-dimensional array.  This function is to
   read a part of the array.  All values read by this function are packed
   together in row-major ordering, which is the typical C/C++ array
   ordering.

   @arg context: an opaque pointer used to stored the context information
   for the source data.  This argument came from the user and is given back
   to the user without ever been updated or modified.
   @arg nd: the number of dimensions of the data array.
   @arg starts: buffer for nd integers designateing the starting  point the
   nd-dimensional subcube.
   @arg counts: buffer for nd integers designating the extents of the
   nd-dimensional subcube.
   @arg data: pointer to the output buffer for the data values to be read
   into memory.  The nd-dimensional subcube is packed into a linear buffer
   in row-major ordering.

   @return >= 0 to indicate success, < 0 to indicate error.
 */
typedef int (*FastBitReadExtArray)
(void *context, uint64_t nd, uint64_t *starts, uint64_t *counts, void *data);

//
// functions for case-insensitive string comparisons
//
#ifdef _WIN32
#  if _MSC_VER >= 1500 || defined(__MINGW__) || defined(__MINGW32__) || defined(__MINGW64__)
#    define strnicmp _strnicmp
#    define stricmp _stricmp
#  endif
#else
#  include <strings.h>	// strcasecmp, strncasecmp
#  define MessageBox(x1,x2,x3,x4); {} // fake message box
#  define strnicmp strncasecmp
#  define stricmp strcasecmp
#endif

// C++ portion
#ifdef __cplusplus

#if defined(__SUNPRO_CC)
#  if (__SUNPRO_CC < 0x500)
#    include <iostream.h>
     typedef int bool;
#    define false 0
#    define true 1
#    define std
#    define mutable
#    define explicit
#  else
#    include <iosfwd>	// std::cout, std::clog
#  endif
#else
#  include <iosfwd>	// std::cout, std::clog
#endif
// #ifndef REASON
// #  define REASON " " << strerror(errno) << std::endl;
// #endif // ifndef REASON

#include <vector>	// std::vector
#include <functional>	// std::less, std::binary_function<>
// namespace of ibis contains most of the useful classes of the implementation
namespace ibis { // forward definition of all the classes in IBIS
    /// @defgroup FastBitMain FastBit IBIS main interface objects.
    /// @{
    class part;		///!< To store information about a data partition.
    class query;	///!< To store information about a query.
    class qExpr;	///!< The base class of query expressions.
    /// @}

    template<class T> class array_t;
    /// A simple list of data partitions.
    typedef FASTBIT_CXX_DLLSPEC std::vector< part* > partList;
    /// A simple list of data partitions.
    typedef FASTBIT_CXX_DLLSPEC std::vector< const part* > constPartList;

    /// The object identifiers used to distinguish records.
    union FASTBIT_CXX_DLLSPEC rid_t {
	uint64_t value;	///!< As a single 64-bit value.
	/// As two 32-bit values.
	struct name {
	    uint32_t run;	///!< Run number.  More significant.
	    uint32_t event;	///!< Event number.  Less significant.
	} num;

	// (num.run < r.num.run) |
	// (num.run == r.num.run && num.event < r.num.event))
	bool operator<(const rid_t& r) const  {return(value < r.value);}
	bool operator>(const rid_t& r) const  {return(value > r.value);}
	bool operator<=(const rid_t& r) const {return(value <= r.value);}
	bool operator>=(const rid_t& r) const {return(value >= r.value);}
	bool operator==(const rid_t& r) const {return(value == r.value);}
	bool operator!=(const rid_t& r) const {return(value != r.value);}
        rid_t operator+(const rid_t& r) const {
            rid_t tmp; tmp.value = value + r.value; return tmp;}
        rid_t operator-(const rid_t& r) const {
            rid_t tmp; tmp.value = value - r.value; return tmp;}
        rid_t operator*(const rid_t& r) const {
            rid_t tmp; tmp.value = value * r.value; return tmp;}
        rid_t operator/(const rid_t& r) const {
            rid_t tmp; tmp.value = value / r.value; return tmp;}
    }; // rid_t

    /// A simple class representing an opaque object.
    class FASTBIT_CXX_DLLSPEC opaque {
    public:
	/// Return the content of the opaque object as a sequence of bytes.
	const char* address() const {return buf_;}
	/// The number of bytes pointed by address.
	uint64_t size() const {return len_;}
	int copy(const void* ptr, uint64_t len);
	/// Assign the external storage to this object.  This object takes
	/// on the responsibility of freeing the pointer ptr and the caller
	/// should not attempt to free ptr.
	///
	/// It returns 0 upon successful completion of the operation,
	/// otherwise, it returns a negative number to indicate error.
	///
	/// @note The pointer ptr must be created with operator new.
	void assign(void* ptr, uint64_t len)  {
	    delete [] buf_;
	    buf_ = static_cast<char*>(ptr);
	    len_ = len;
	}
	/// Assign the content from rhs to this.
	void assign(opaque &rhs) {
	    delete [] buf_;
	    buf_ = rhs.buf_;
	    len_ = rhs.len_;
	    rhs.buf_ = 0;
	    rhs.len_ = 0;
	}

	/// Swap the content of two opaque objects.
	void swap(opaque& rhs) {
	    char* ptr = buf_;
	    buf_ = rhs.buf_;
	    rhs.buf_ = ptr;
	    uint64_t len = len_;
	    len_ = rhs.len_;
	    rhs.len_ = len;
	}

	/// Destructor.
	~opaque() {delete [] buf_;}
	/// The default constructor.
	opaque() : buf_(0), len_(0) {};
	/// Constructor.  The extenal buffer is given to the new object to
	/// manage and the pointer ptr must be created with operator new.
	opaque(void* ptr, uint64_t len)
	    : buf_(static_cast<char*>(ptr)), len_(len) {}

	/// Copy constructor.  Performs a deep copy.
	opaque(const opaque &rhs) : buf_(0), len_(0) {
	    copy(rhs.buf_, rhs.len_);
	}
	/// Assignment operator.  Performs a deep copy.
	opaque& operator=(const opaque &rhs) {
	    copy(rhs.buf_, rhs.len_);
	    return *this;
	}

    protected:
	char* buf_;
	uint64_t len_;
    }; // opaque

    /// A case-insensitive version of less for comparing names of tables,
    /// columns, and other resources.
    struct lessi :
	public std::binary_function< const char*, const char*, bool > {
	bool operator()(const char* x, const char* y) const {
	    return (x && y ? stricmp(x, y) < 0 : false);
	}
    }; // lessi

    /// Verbosity level.  The larger the value, the more is printed.
    /// The default value is 0.  A negative value will disable all printing.
    extern FASTBIT_CXX_DLLSPEC int gVerbose;
} // namespace ibis
#endif // C++ portion
#endif // ifndef IBIS_CONST_H
