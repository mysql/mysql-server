/*
 * $Id: acconfig.h,v 11.29 2000/09/20 16:30:33 bostic Exp $
 */

/* Define if you are building a version for running the test suite. */
#undef CONFIG_TEST

/* Define if you want a debugging version. */
#undef DEBUG

/* Define if you want a version that logs read operations. */
#undef DEBUG_ROP

/* Define if you want a version that logs write operations. */
#undef DEBUG_WOP

/* Define if you want a version with run-time diagnostic checking. */
#undef DIAGNOSTIC

/* Define if you want to mask harmless unitialized memory read/writes. */
#undef UMRW

/* Define if fcntl/F_SETFD denies child access to file descriptors. */
#undef HAVE_FCNTL_F_SETFD

/* Define if building big-file environment (e.g., AIX, HP/UX, Solaris). */
#undef HAVE_FILE_OFFSET_BITS

/* Mutex possibilities. */
#undef HAVE_MUTEX_68K_GCC_ASSEMBLY
#undef HAVE_MUTEX_AIX_CHECK_LOCK
#undef HAVE_MUTEX_ALPHA_GCC_ASSEMBLY
#undef HAVE_MUTEX_HPPA_GCC_ASSEMBLY
#undef HAVE_MUTEX_HPPA_MSEM_INIT
#undef HAVE_MUTEX_IA64_GCC_ASSEMBLY
#undef HAVE_MUTEX_MACOS
#undef HAVE_MUTEX_MSEM_INIT
#undef HAVE_MUTEX_PPC_GCC_ASSEMBLY
#undef HAVE_MUTEX_PTHREADS
#undef HAVE_MUTEX_RELIANTUNIX_INITSPIN
#undef HAVE_MUTEX_SCO_X86_CC_ASSEMBLY
#undef HAVE_MUTEX_SEMA_INIT
#undef HAVE_MUTEX_SGI_INIT_LOCK
#undef HAVE_MUTEX_SOLARIS_LOCK_TRY
#undef HAVE_MUTEX_SOLARIS_LWP
#undef HAVE_MUTEX_SPARC_GCC_ASSEMBLY
#undef HAVE_MUTEX_THREADS
#undef HAVE_MUTEX_UI_THREADS
#undef HAVE_MUTEX_UTS_CC_ASSEMBLY
#undef HAVE_MUTEX_VMS
#undef HAVE_MUTEX_VXWORKS
#undef HAVE_MUTEX_WIN16
#undef HAVE_MUTEX_WIN32
#undef HAVE_MUTEX_X86_GCC_ASSEMBLY

/* Define if building on QNX. */
#undef HAVE_QNX

/* Define if building RPC client/server. */
#undef HAVE_RPC

/* Define if your sprintf returns a pointer, not a length. */
#undef SPRINTF_RET_CHARPNT

@BOTTOM@

/*
 * Big-file configuration.
 */
#ifdef	HAVE_FILE_OFFSET_BITS
#define	_FILE_OFFSET_BITS	64
#endif

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
