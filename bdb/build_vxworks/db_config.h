/*
 * $Id: db_config.h,v 1.4 2000/12/12 18:39:26 bostic Exp $
 */

/* Define if building VxWorks */
#define HAVE_VXWORKS 1

/* Define to empty if the keyword does not work.  */
/* #undef const */

/* Define if your struct stat has st_blksize.  */
#define HAVE_ST_BLKSIZE 1

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

/* !!!
 * WORDS_BIGENDIAN is the ONLY option in this file that may be edited
 * for VxWorks.
 *
 * The user must set this according to VxWork's target arch.  We use an
 * x86 (little-endian) target.
 */
/* Define if your processor stores words with the most significant
   byte first (like Motorola and SPARC, unlike Intel and VAX).  */
/* #undef WORDS_BIGENDIAN */

/* !!!
 * The CONFIG_TEST option may be added using the Tornado project build.
 * DO NOT modify it here.
 */
/* Define if you are building a version for running the test suite. */
/* #undef CONFIG_TEST */

/* !!!
 * The DEBUG option may be added using the Tornado project build.
 * DO NOT modify it here.
 */
/* Define if you want a debugging version. */
/* #undef DEBUG */

/* Define if you want a version that logs read operations. */
/* #undef DEBUG_ROP */

/* Define if you want a version that logs write operations. */
/* #undef DEBUG_WOP */

/* !!!
 * The DIAGNOSTIC option may be added using the Tornado project build.
 * DO NOT modify it here.
 */
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
#define HAVE_MUTEX_VXWORKS 1
/* #undef HAVE_MUTEX_WIN16 */
/* #undef HAVE_MUTEX_WIN32 */
/* #undef HAVE_MUTEX_X86_GCC_ASSEMBLY */

/* Define if building on QNX. */
/* #undef HAVE_QNX */

/* !!!
 * The HAVE_RPC option may be added using the Tornado project build.
 * DO NOT modify it here.
 */
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
#define HAVE_MLOCK 1

/* Define if you have the mmap function.  */
/* #undef HAVE_MMAP */

/* Define if you have the munlock function.  */
#define HAVE_MUNLOCK 1

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
#define HAVE_SCHED_YIELD 1

/* Define if you have the select function.  */
#define HAVE_SELECT 1

/* Define if you have the shmget function.  */
/* #undef HAVE_SHMGET */

/* Define if you have the snprintf function.  */
/* #undef HAVE_SNPRINTF */

/* Define if you have the strcasecmp function.  */
/* #undef HAVE_STRCASECMP */

/* Define if you have the strerror function.  */
#define HAVE_STRERROR 1

/* Define if you have the strtoul function.  */
#define HAVE_STRTOUL 1

/* Define if you have the sysconf function.  */
/* #undef HAVE_SYSCONF */

/* Define if you have the vsnprintf function.  */
/* #undef HAVE_VSNPRINTF */

/* Define if you have the yield function.  */
/* #undef HAVE_YIELD */

/* Define if you have the <dirent.h> header file.  */
#define HAVE_DIRENT_H 1

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
 * !!!
 * The following is not part of the automatic configuration setup, but
 * provides necessary VxWorks information.
 */
#include "vxWorks.h"

/*
 * VxWorks does not have getpid().
 */
#define	getpid()	taskIdSelf()

/*
 * Don't step on the namespace.  Other libraries may have their own
 * implementations of these functions, we don't want to use their
 * implementations or force them to use ours based on the load order.
 */
#ifndef	HAVE_GETCWD
#define	getcwd		__db_Cgetcwd
#endif
#ifndef	HAVE_GETOPT
#define	getopt		__db_Cgetopt
#endif
#ifndef	HAVE_MEMCMP
#define	memcmp		__db_Cmemcmp
#endif
#ifndef	HAVE_MEMCPY
#define	memcpy		__db_Cmemcpy
#endif
#ifndef	HAVE_MEMMOVE
#define	memmove		__db_Cmemmove
#endif
#ifndef	HAVE_RAISE
#define	raise		__db_Craise
#endif
#ifndef	HAVE_SNPRINTF
#define	snprintf	__db_Csnprintf
#endif
#ifndef	HAVE_STRCASECMP
#define	strcasecmp	__db_Cstrcasecmp
#endif
#ifndef	HAVE_STRERROR
#define	strerror	__db_Cstrerror
#endif
#ifndef	HAVE_VSNPRINTF
#define	vsnprintf	__db_Cvsnprintf
#endif
