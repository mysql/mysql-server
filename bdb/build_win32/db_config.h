/*
 * $Id: db_config.h,v 11.24 2000/12/12 18:39:26 bostic Exp $
 */

/* Define to empty if the keyword does not work.  */
/* #undef const */

/* Define if your struct stat has st_blksize.  */
/* #undef HAVE_ST_BLKSIZE */

/* Define to `int' if <sys/types.h> doesn't define.  */
/* #undef mode_t */

/* Define to `long' if <sys/types.h> doesn't define.  */
/* #undef off_t */

/* Define to `int' if <sys/types.h> doesn't define.  */
/* #undef pid_t */

/* Define to `unsigned' if <sys/types.h> doesn't define.  */
/* #undef size_t */

/* Define if the `S_IS*' macros in <sys/stat.h> do not work properly.  */
/* #undef STAT_MACROS_BROKEN */

/* Define if you have the ANSI C header files.  */
#define STDC_HEADERS 1

/* Define if you can safely include both <sys/time.h> and <time.h>.  */
/* #undef TIME_WITH_SYS_TIME */

/* Define if your processor stores words with the most significant
   byte first (like Motorola and SPARC, unlike Intel and VAX).  */
/* #undef WORDS_BIGENDIAN */

/* Define if you are building a version for running the test suite. */
/* #undef CONFIG_TEST */

/* Define if you want a debugging version. */
/* #undef DEBUG */
#if defined(_DEBUG)
#if !defined(DEBUG)
#define DEBUG 1
#endif
#endif

/* Define if you want a version that logs read operations. */
/* #undef DEBUG_ROP */

/* Define if you want a version that logs write operations. */
/* #undef DEBUG_WOP */

/* Define if you want a version with run-time diagnostic checking. */
/* #undef DIAGNOSTIC */

/* Define if you want to mask harmless unitialized memory read/writes. */
/* #undef UMRW */

/* Define if fcntl/F_SETFD denies child access to file descriptors. */
/* #undef HAVE_FCNTL_F_SETFD */

/* Define if building big-file environment (e.g., AIX, HP/UX, Solaris). */
/* #undef HAVE_FILE_OFFSET_BITS */

/* Mutex possibilities. */
/* #undef HAVE_MUTEX_68K_GCC_ASSEMBLY */
/* #undef HAVE_MUTEX_AIX_CHECK_LOCK */
/* #undef HAVE_MUTEX_ALPHA_GCC_ASSEMBLY */
/* #undef HAVE_MUTEX_HPPA_GCC_ASSEMBLY */
/* #undef HAVE_MUTEX_HPPA_MSEM_INIT */
/* #undef HAVE_MUTEX_IA64_GCC_ASSEMBLY */
/* #undef HAVE_MUTEX_MACOS */
/* #undef HAVE_MUTEX_MSEM_INIT */
/* #undef HAVE_MUTEX_PPC_GCC_ASSEMBLY */
/* #undef HAVE_MUTEX_PTHREADS */
/* #undef HAVE_MUTEX_RELIANTUNIX_INITSPIN */
/* #undef HAVE_MUTEX_SCO_X86_CC_ASSEMBLY */
/* #undef HAVE_MUTEX_SEMA_INIT */
/* #undef HAVE_MUTEX_SGI_INIT_LOCK */
/* #undef HAVE_MUTEX_SOLARIS_LOCK_TRY */
/* #undef HAVE_MUTEX_SOLARIS_LWP */
/* #undef HAVE_MUTEX_SPARC_GCC_ASSEMBLY */
#define HAVE_MUTEX_THREADS 1
/* #undef HAVE_MUTEX_UI_THREADS */
/* #undef HAVE_MUTEX_UTS_CC_ASSEMBLY */
/* #undef HAVE_MUTEX_VMS */
/* #undef HAVE_MUTEX_VXWORKS */
/* #undef HAVE_MUTEX_WIN16 */
#define HAVE_MUTEX_WIN32 1
/* #undef HAVE_MUTEX_X86_GCC_ASSEMBLY */

/* Define if building on QNX. */
/* #undef HAVE_QNX */

/* Define if building RPC client/server. */
/* #undef HAVE_RPC */

/* Define if your sprintf returns a pointer, not a length. */
/* #undef SPRINTF_RET_CHARPNT */

/* Define if you have the getcwd function.  */
#define HAVE_GETCWD 1

/* Define if you have the getopt function.  */
/* #undef HAVE_GETOPT */

/* Define if you have the getuid function.  */
/* #undef HAVE_GETUID */

/* Define if you have the memcmp function.  */
#define HAVE_MEMCMP 1

/* Define if you have the memcpy function.  */
#define HAVE_MEMCPY 1

/* Define if you have the memmove function.  */
#define HAVE_MEMMOVE 1

/* Define if you have the mlock function.  */
/* #undef HAVE_MLOCK */

/* Define if you have the mmap function.  */
/* #undef HAVE_MMAP */

/* Define if you have the munlock function.  */
/* #undef HAVE_MUNLOCK */

/* Define if you have the munmap function.  */
/* #undef HAVE_MUNMAP */

/* Define if you have the pread function.  */
/* #undef HAVE_PREAD */

/* Define if you have the pstat_getdynamic function.  */
/* #undef HAVE_PSTAT_GETDYNAMIC */

/* Define if you have the pwrite function.  */
/* #undef HAVE_PWRITE */

/* Define if you have the qsort function.  */
#define HAVE_QSORT 1

/* Define if you have the raise function.  */
#define HAVE_RAISE 1

/* Define if you have the sched_yield function.  */
/* #undef HAVE_SCHED_YIELD */

/* Define if you have the select function.  */
/* #undef HAVE_SELECT */

/* Define if you have the shmget function.  */
/* #undef HAVE_SHMGET */

/* Define if you have the snprintf function.  */
#define HAVE_SNPRINTF 1

/* Define if you have the strcasecmp function.  */
/* #undef HAVE_STRCASECMP */

/* Define if you have the strerror function.  */
#define HAVE_STRERROR 1

/* Define if you have the strtoul function.  */
#define HAVE_STRTOUL 1

/* Define if you have the sysconf function.  */
/* #undef HAVE_SYSCONF */

/* Define if you have the vsnprintf function.  */
#define HAVE_VSNPRINTF 1

/* Define if you have the yield function.  */
/* #undef HAVE_YIELD */

/* Define if you have the <dirent.h> header file.  */
/* #undef HAVE_DIRENT_H */

/* Define if you have the <ndir.h> header file.  */
/* #undef HAVE_NDIR_H */

/* Define if you have the <sys/dir.h> header file.  */
/* #undef HAVE_SYS_DIR_H */

/* Define if you have the <sys/ndir.h> header file.  */
/* #undef HAVE_SYS_NDIR_H */

/* Define if you have the <sys/select.h> header file.  */
/* #undef HAVE_SYS_SELECT_H */

/* Define if you have the <sys/time.h> header file.  */
/* #undef HAVE_SYS_TIME_H */

/* Define if you have the nsl library (-lnsl).  */
/* #undef HAVE_LIBNSL */

/*
 * XXX
 * The following is not part of the automatic configuration setup,
 * but provides the information necessary to build DB on Windows.
 */
#include <sys/types.h>
#include <sys/stat.h>

#include <direct.h>
#include <fcntl.h>
#include <io.h>
#include <limits.h>
#include <memory.h>
#include <process.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>

#if defined(__cplusplus)
#include <iostream.h>
#endif

/*
 * To build Tcl interface libraries, the include path must be configured to
 * use the directory containing <tcl.h>, usually the include directory in
 * the Tcl distribution.
 */
#ifdef DB_TCL_SUPPORT
#include <tcl.h>
#endif

#define	WIN32_LEAN_AND_MEAN
#include <windows.h>

/*
 * Win32 has fsync, getcwd, snprintf and vsnprintf, but under different names.
 */
#define	fsync(fd)		_commit(fd)
#define	getcwd(buf, size)	_getcwd(buf, size)
#define	snprintf		_snprintf
#define	vsnprintf		_vsnprintf

/*
 * Win32 does not define getopt and friends in any header file, so we must.
 */
#if defined(__cplusplus)
extern "C" {
#endif
extern int optind;
extern char *optarg;
extern int getopt(int, char * const *, const char *);
#if defined(__cplusplus)
}
#endif

#define	NO_SYSTEM_INCLUDES

/*
 * We use DB_WIN32 much as one would use _WIN32, to determine that we're
 * using an operating system environment that supports Win32 calls
 * and semantics.  We don't use _WIN32 because cygwin/gcc also defines
 * that, even though it closely emulates the Unix environment.
 */
#define DB_WIN32 1

/*
 * This is a grievous hack -- once we've included windows.h, we have no choice
 * but to use ANSI-style varargs (because it pulls in stdarg.h for us).  DB's
 * code decides which type of varargs to use based on the state of __STDC__.
 * Sensible.  Unfortunately, Microsoft's compiler _doesn't_ define __STDC__
 * unless you invoke it with arguments turning OFF all vendor extensions.  Even
 * more unfortunately, if we do that, it fails to parse windows.h!!!!!  So, we
 * define __STDC__ here, after windows.h comes in.  Note: the compiler knows
 * we've defined it, and starts enforcing strict ANSI compilance from this point
 * on.
 */
#define __STDC__ 1
