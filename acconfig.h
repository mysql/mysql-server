/* acconfig.h
   This file is in the public domain.

   Descriptive text for the C preprocessor macros that
   the distributed Autoconf macros can define.
   No software package will use all of them; autoheader copies the ones
   your configure.in uses into your configuration header file templates.

   The entries are in sort -df order: alphabetical, case insensitive,
   ignoring punctuation (such as underscores).  Although this order
   can split up related entries, it makes it easier to check whether
   a given entry is in the file.

   Leave the following blank line there!!  Autoheader needs it.  */


/* Version of .frm files */
#undef DOT_FRM_VERSION

/* READLINE: */
#undef FIONREAD_IN_SYS_IOCTL

/* READLINE: Define if your system defines TIOCGWINSZ in sys/ioctl.h.  */
#undef GWINSZ_IN_SYS_IOCTL

/* Handing of large files on Solaris 2.6 */
#undef _FILE_OFFSET_BITS

/* Do we have FIONREAD */
#undef FIONREAD_IN_SYS_IOCTL

/* Do we need to define _GNU_SOURCE */
#undef _GNU_SOURCE

/* atomic_add() from <asm/atomic.h> (Linux only) */
#undef HAVE_ATOMIC_ADD

/* atomic_sub() from <asm/atomic.h> (Linux only) */
#undef HAVE_ATOMIC_SUB

/* bool is not defined by all C++ compilators */
#undef HAVE_BOOL

/* Have berkeley db installed */
#undef HAVE_BERKELEY_DB

/* DSB style signals ? */
#undef HAVE_BSD_SIGNALS

/* Can netinet be included */
#undef HAVE_BROKEN_NETINET_INCLUDES

/* READLINE: */
#undef HAVE_BSD_SIGNALS

/* ZLIB and compress: */
#undef HAVE_COMPRESS

/* Define if we are using OSF1 DEC threads */
#undef HAVE_DEC_THREADS

/* Define if we are using OSF1 DEC threads on 3.2 */
#undef HAVE_DEC_3_2_THREADS

/* fp_except from ieeefp.h */
#undef HAVE_FP_EXCEPT

/* READLINE: */
#undef HAVE_GETPW_DECLS

/* Solaris define gethostbyname_r with 5 arguments. glibc2 defines
   this with 6 arguments */
#undef HAVE_GETHOSTBYNAME_R_GLIBC2_STYLE

/* In OSF 4.0f the 3'd argument to gethostname_r is hostent_data * */
#undef HAVE_GETHOSTBYNAME_R_RETURN_INT

/* Define if int8, int16 and int32 types exist */
#undef HAVE_INT_8_16_32

/* Using Innobase DB */
#undef HAVE_INNOBASE_DB

/* Define if we have GNU readline */
#undef HAVE_LIBREADLINE

/* Define if have -lwrap */
#undef HAVE_LIBWRAP

/* Define if we are using Xavier Leroy's LinuxThreads */
#undef HAVE_LINUXTHREADS

/* Do we have lstat */
#undef HAVE_LSTAT

/* Do we use user level threads */
#undef HAVE_mit_thread

/* For some non posix threads */
#undef HAVE_NONPOSIX_PTHREAD_GETSPECIFIC

/* For some non posix threads */
#undef HAVE_NONPOSIX_PTHREAD_MUTEX_INIT

/* READLINE: */
#undef HAVE_POSIX_SIGNALS

/* Well.. */
#undef HAVE_POSIX_SIGSETJMP

/* sigwait with one argument */
#undef HAVE_NONPOSIX_SIGWAIT

/* pthread_attr_setscope */
#undef HAVE_PTHREAD_ATTR_SETSCOPE

/* POSIX readdir_r */
#undef HAVE_READDIR_R

/* Have Gemini db installed */
#undef HAVE_GEMINI_DB

/* POSIX sigwait */
#undef HAVE_SIGWAIT

/* crypt */
#undef HAVE_CRYPT

/* Solaris define gethostbyaddr_r with 7 arguments. glibc2 defines
   this with 8 arguments */
#undef HAVE_SOLARIS_STYLE_GETHOST

/* MIT pthreads does not support connecting with unix sockets */
#undef HAVE_THREADS_WITHOUT_SOCKETS

/* Timespec has a ts_sec instead of tv_sev  */
#undef HAVE_TIMESPEC_TS_SEC

/* Have the tzname variable */
#undef HAVE_TZNAME

/* Define if the system files define uchar */
#undef HAVE_UCHAR

/* Define if the system files define uint */
#undef HAVE_UINT

/* Define if the system files define ulong */
#undef HAVE_ULONG

/* UNIXWARE7 threads are not posix */
#undef HAVE_UNIXWARE7_THREADS

/* new UNIXWARE7 threads that are not yet posix */
#undef HAVE_UNIXWARE7_POSIX

/* READLINE: */
#undef HAVE_USG_SIGHOLD

/* Handling of large files on Solaris 2.6 */
#undef _LARGEFILE_SOURCE

/* Handling of large files on Solaris 2.6 */
#undef _LARGEFILE64_SOURCE

/* Define if want -lwrap */
#undef LIBWRAP

/* Define to machine type name eg sun10 */
#undef MACHINE_TYPE

#undef MUST_REINSTALL_SIGHANDLERS

/* Defined to used character set */
#undef MY_CHARSET_CURRENT

/* READLINE: no sys file*/
#undef NO_SYS_FILE

/* Program name */
#undef PACKAGE

/* mysql client protocoll version */
#undef PROTOCOL_VERSION

/* Define if qsort returns void */
#undef QSORT_TYPE_IS_VOID

/* Define as the return type of qsort (int or void). */
#undef RETQSORTTYPE

/* Size of off_t */
#undef SIZEOF_OFF_T

/* Define as the base type of the last arg to accept */
#undef SOCKET_SIZE_TYPE

/* Last argument to get/setsockopt */
#undef SOCKOPT_OPTLEN_TYPE

#undef SPEED_T_IN_SYS_TYPES
#undef SPRINTF_RETURNS_PTR
#undef SPRINTF_RETURNS_INT
#undef SPRINTF_RETURNS_GARBAGE

/* Needed to get large file supportat HPUX 10.20 */
#undef __STDC_EXT__

#undef STRCOLL_BROKEN

#undef STRUCT_DIRENT_HAS_D_FILENO
#undef STRUCT_DIRENT_HAS_D_INO

#undef STRUCT_WINSIZE_IN_SYS_IOCTL
#undef STRUCT_WINSIZE_IN_TERMIOS

/* Define to name of system eg solaris*/
#undef SYSTEM_TYPE

/* Define if you want to have threaded code. This may be undef on client code */
#undef THREAD

/* Should be client be thread safe */
#undef THREAD_SAFE_CLIENT

/* READLINE: */
#undef TIOCSTAT_IN_SYS_IOCTL

/* Use multi-byte character routines */
#undef USE_MB
#undef USE_MB_IDENT

/* Use MySQL RAID */
#undef USE_RAID

/* Use strcoll() functions when comparing and sorting. */
#undef USE_STRCOLL

/* Program version */
#undef VERSION

/* READLINE: */
#undef VOID_SIGHANDLER


/* Leave that blank line there!!  Autoheader needs it.
   If you're adding to this file, keep in mind:
   The entries are in sort -df order: alphabetical, case insensitive,
   ignoring punctuation (such as underscores).  */
