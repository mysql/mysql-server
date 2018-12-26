/* Copyright (c) 2009, 2018, Oracle and/or its affiliates. All rights reserved.
 
 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; version 2 of the License.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#ifndef MY_CONFIG_H
#define MY_CONFIG_H

/*
 * From configure.cmake, in order of appearance 
 */
#cmakedefine HAVE_LLVM_LIBCPP 1
#cmakedefine _LARGEFILE_SOURCE 1

/* Libraries */
#cmakedefine HAVE_LIBM 1
#cmakedefine HAVE_LIBNSL 1
#cmakedefine HAVE_LIBCRYPT 1
#cmakedefine HAVE_LIBSOCKET 1
#cmakedefine HAVE_LIBDL 1
#cmakedefine HAVE_LIBRT 1
#cmakedefine HAVE_LIBWRAP 1
#cmakedefine HAVE_LIBWRAP_PROTOTYPES 1

/* Header files */
#cmakedefine HAVE_ALLOCA_H 1
#cmakedefine HAVE_ARPA_INET_H 1
#cmakedefine HAVE_CRYPT_H 1
#cmakedefine HAVE_DLFCN_H 1
#cmakedefine HAVE_EXECINFO_H 1
#cmakedefine HAVE_FPU_CONTROL_H 1
#cmakedefine HAVE_GRP_H 1
#cmakedefine HAVE_IEEEFP_H 1
#cmakedefine HAVE_LANGINFO_H 1
#cmakedefine HAVE_MALLOC_H 1
#cmakedefine HAVE_NETINET_IN_H 1
#cmakedefine HAVE_POLL_H 1
#cmakedefine HAVE_PWD_H 1
#cmakedefine HAVE_STRINGS_H 1
#cmakedefine HAVE_SYS_CDEFS_H 1
#cmakedefine HAVE_SYS_IOCTL_H 1
#cmakedefine HAVE_SYS_MMAN_H 1
#cmakedefine HAVE_SYS_RESOURCE_H 1
#cmakedefine HAVE_SYS_SELECT_H 1
#cmakedefine HAVE_SYS_SOCKET_H 1
#cmakedefine HAVE_TERM_H 1
#cmakedefine HAVE_TERMIOS_H 1
#cmakedefine HAVE_TERMIO_H 1
#cmakedefine HAVE_UNISTD_H 1
#cmakedefine HAVE_SYS_WAIT_H 1
#cmakedefine HAVE_SYS_PARAM_H 1
#cmakedefine HAVE_FNMATCH_H 1
#cmakedefine HAVE_SYS_UN_H 1
#cmakedefine HAVE_VIS_H 1
#cmakedefine HAVE_SASL_SASL_H 1

/* Libevent */
#cmakedefine HAVE_DEVPOLL 1
#cmakedefine HAVE_SYS_DEVPOLL_H 1
#cmakedefine HAVE_SYS_EPOLL_H 1
#cmakedefine HAVE_TAILQFOREACH 1

/* Functions */
#cmakedefine HAVE_ALIGNED_MALLOC 1
#cmakedefine HAVE_BACKTRACE 1
#cmakedefine HAVE_PRINTSTACK 1
#cmakedefine HAVE_INDEX 1
#cmakedefine HAVE_CLOCK_GETTIME 1
#cmakedefine HAVE_CUSERID 1
#cmakedefine HAVE_DIRECTIO 1
#cmakedefine HAVE_FTRUNCATE 1
#cmakedefine HAVE_COMPRESS 1
#cmakedefine HAVE_CRYPT 1
#cmakedefine HAVE_DLOPEN 1
#cmakedefine HAVE_FCHMOD 1
#cmakedefine HAVE_FCNTL 1
#cmakedefine HAVE_FDATASYNC 1
#cmakedefine HAVE_DECL_FDATASYNC 1 
#cmakedefine HAVE_FEDISABLEEXCEPT 1
#cmakedefine HAVE_FSEEKO 1
#cmakedefine HAVE_FSYNC 1
#cmakedefine HAVE_GETHOSTBYADDR_R 1
#cmakedefine HAVE_GETHRTIME 1
#cmakedefine HAVE_GETNAMEINFO 1
#cmakedefine HAVE_GETPASS 1
#cmakedefine HAVE_GETPASSPHRASE 1
#cmakedefine HAVE_GETPWNAM 1
#cmakedefine HAVE_GETPWUID 1
#cmakedefine HAVE_GETRLIMIT 1
#cmakedefine HAVE_GETRUSAGE 1
#cmakedefine HAVE_INITGROUPS 1
#cmakedefine HAVE_ISSETUGID 1
#cmakedefine HAVE_GETUID 1
#cmakedefine HAVE_GETEUID 1
#cmakedefine HAVE_GETGID 1
#cmakedefine HAVE_GETEGID 1
#cmakedefine HAVE_LSTAT 1
#cmakedefine HAVE_MADVISE 1
#cmakedefine HAVE_MALLOC_INFO 1
#cmakedefine HAVE_MEMRCHR 1
#cmakedefine HAVE_MLOCK 1
#cmakedefine HAVE_MLOCKALL 1
#cmakedefine HAVE_MMAP64 1
#cmakedefine HAVE_POLL 1
#cmakedefine HAVE_POSIX_FALLOCATE 1
#cmakedefine HAVE_POSIX_MEMALIGN 1
#cmakedefine HAVE_PREAD 1
#cmakedefine HAVE_PTHREAD_CONDATTR_SETCLOCK 1
#cmakedefine HAVE_PTHREAD_SIGMASK 1
#cmakedefine HAVE_READLINK 1
#cmakedefine HAVE_REALPATH 1
#cmakedefine HAVE_SETFD 1
#cmakedefine HAVE_SIGACTION 1
#cmakedefine HAVE_SLEEP 1
#cmakedefine HAVE_STPCPY 1
#cmakedefine HAVE_STPNCPY 1
#cmakedefine HAVE_STRLCPY 1
#cmakedefine HAVE_STRNLEN 1
#cmakedefine HAVE_STRLCAT 1
#cmakedefine HAVE_STRSIGNAL 1
#cmakedefine HAVE_FGETLN 1
#cmakedefine HAVE_STRSEP 1
#cmakedefine HAVE_TELL 1
#cmakedefine HAVE_VASPRINTF 1
#cmakedefine HAVE_MEMALIGN 1
#cmakedefine HAVE_NL_LANGINFO 1
#cmakedefine HAVE_HTONLL 1
#cmakedefine DNS_USE_CPU_CLOCK_FOR_ID 1
#cmakedefine HAVE_EPOLL 1
/* #cmakedefine HAVE_EVENT_PORTS 1 */
#cmakedefine HAVE_INET_NTOP 1
#cmakedefine HAVE_WORKING_KQUEUE 1
#cmakedefine HAVE_TIMERADD 1
#cmakedefine HAVE_TIMERCLEAR 1
#cmakedefine HAVE_TIMERCMP 1
#cmakedefine HAVE_TIMERISSET 1

/* WL2373 */
#cmakedefine HAVE_SYS_TIME_H 1
#cmakedefine HAVE_SYS_TIMES_H 1
#cmakedefine HAVE_TIMES 1
#cmakedefine HAVE_GETTIMEOFDAY 1

/* Symbols */
#cmakedefine HAVE_LRAND48 1
#cmakedefine GWINSZ_IN_SYS_IOCTL 1
#cmakedefine FIONREAD_IN_SYS_IOCTL 1
#cmakedefine FIONREAD_IN_SYS_FILIO 1
#cmakedefine HAVE_SIGEV_THREAD_ID 1
#cmakedefine HAVE_SIGEV_PORT 1
#cmakedefine HAVE_LOG2 1

#cmakedefine HAVE_ISINF 1

#cmakedefine HAVE_KQUEUE_TIMERS 1
#cmakedefine HAVE_POSIX_TIMERS 1

/* Endianess */
#cmakedefine WORDS_BIGENDIAN 1 

/* Type sizes */
#cmakedefine SIZEOF_VOIDP     @SIZEOF_VOIDP@
#cmakedefine SIZEOF_CHARP     @SIZEOF_CHARP@
#cmakedefine SIZEOF_LONG      @SIZEOF_LONG@
#cmakedefine SIZEOF_SHORT     @SIZEOF_SHORT@
#cmakedefine SIZEOF_INT       @SIZEOF_INT@
#cmakedefine SIZEOF_LONG_LONG @SIZEOF_LONG_LONG@
#cmakedefine SIZEOF_OFF_T     @SIZEOF_OFF_T@
#cmakedefine SIZEOF_TIME_T    @SIZEOF_TIME_T@
#cmakedefine HAVE_UINT 1
#cmakedefine HAVE_ULONG 1
#cmakedefine HAVE_U_INT32_T 1
#cmakedefine HAVE_STRUCT_TIMESPEC

/* Support for tagging symbols with __attribute__((visibility("hidden"))) */
#cmakedefine HAVE_VISIBILITY_HIDDEN 1

/* Code tests*/
#cmakedefine STACK_DIRECTION @STACK_DIRECTION@
#cmakedefine TIME_WITH_SYS_TIME 1
#cmakedefine NO_FCNTL_NONBLOCK 1
#cmakedefine HAVE_PAUSE_INSTRUCTION 1
#cmakedefine HAVE_FAKE_PAUSE_INSTRUCTION 1
#cmakedefine HAVE_HMT_PRIORITY_INSTRUCTION 1
#cmakedefine HAVE_ABI_CXA_DEMANGLE 1
#cmakedefine HAVE_BUILTIN_UNREACHABLE 1
#cmakedefine HAVE_BUILTIN_EXPECT 1
#cmakedefine HAVE_BUILTIN_STPCPY 1
#cmakedefine HAVE_GCC_ATOMIC_BUILTINS 1
#cmakedefine HAVE_GCC_SYNC_BUILTINS 1
#cmakedefine HAVE_VALGRIND
#cmakedefine HAVE_PTHREAD_THREADID_NP 1

/* IPV6 */
#cmakedefine HAVE_NETINET_IN6_H 1
#cmakedefine HAVE_STRUCT_SOCKADDR_IN6 1
#cmakedefine HAVE_STRUCT_IN6_ADDR 1
#cmakedefine HAVE_IPV6 1

#cmakedefine ss_family @ss_family@
#cmakedefine HAVE_SOCKADDR_IN_SIN_LEN 1
#cmakedefine HAVE_SOCKADDR_IN6_SIN6_LEN 1

/*
 * Platform specific CMake files
 */
#define MACHINE_TYPE "@MYSQL_MACHINE_TYPE@"
#cmakedefine HAVE_LINUX_LARGE_PAGES 1
#cmakedefine HAVE_SOLARIS_LARGE_PAGES 1
#cmakedefine HAVE_SOLARIS_ATOMIC 1
#cmakedefine HAVE_SOLARIS_STYLE_GETHOST 1
#define SYSTEM_TYPE "@SYSTEM_TYPE@"
/* Windows stuff, mostly functions, that have Posix analogs but named differently */
#cmakedefine IPPROTO_IPV6 @IPPROTO_IPV6@
#cmakedefine IPV6_V6ONLY @IPV6_V6ONLY@
/* This should mean case insensitive file system */
#cmakedefine FN_NO_CASE_SENSE 1

/*
 * From main CMakeLists.txt
 */
#cmakedefine MAX_INDEXES @MAX_INDEXES@
#cmakedefine WITH_INNODB_MEMCACHED 1
#cmakedefine ENABLE_MEMCACHED_SASL 1
#cmakedefine ENABLE_MEMCACHED_SASL_PWDB 1
#cmakedefine ENABLED_PROFILING 1
#cmakedefine HAVE_ASAN
#cmakedefine ENABLED_LOCAL_INFILE 1
#cmakedefine OPTIMIZER_TRACE 1
#cmakedefine DEFAULT_MYSQL_HOME "@DEFAULT_MYSQL_HOME@"
#cmakedefine SHAREDIR "@SHAREDIR@"
#cmakedefine DEFAULT_BASEDIR "@DEFAULT_BASEDIR@"
#cmakedefine MYSQL_DATADIR "@MYSQL_DATADIR@"
#cmakedefine MYSQL_KEYRINGDIR "@MYSQL_KEYRINGDIR@"
#cmakedefine DEFAULT_CHARSET_HOME "@DEFAULT_CHARSET_HOME@"
#cmakedefine PLUGINDIR "@PLUGINDIR@"
#cmakedefine DEFAULT_SYSCONFDIR "@DEFAULT_SYSCONFDIR@"
#cmakedefine DEFAULT_TMPDIR @DEFAULT_TMPDIR@
#cmakedefine INSTALL_SBINDIR "@default_prefix@/@INSTALL_SBINDIR@"
#cmakedefine INSTALL_BINDIR "@default_prefix@/@INSTALL_BINDIR@"
#cmakedefine INSTALL_MYSQLSHAREDIR "@default_prefix@/@INSTALL_MYSQLSHAREDIR@"
#cmakedefine INSTALL_SHAREDIR "@default_prefix@/@INSTALL_SHAREDIR@"
#cmakedefine INSTALL_PLUGINDIR "@default_prefix@/@INSTALL_PLUGINDIR@"
#cmakedefine INSTALL_INCLUDEDIR "@default_prefix@/@INSTALL_INCLUDEDIR@"
#cmakedefine INSTALL_SCRIPTDIR "@default_prefix@/@INSTALL_SCRIPTDIR@"
#cmakedefine INSTALL_MYSQLDATADIR "@default_prefix@/@INSTALL_MYSQLDATADIR@"
#cmakedefine INSTALL_MYSQLKEYRINGDIR "@default_prefix@/@INSTALL_MYSQLKEYRINGDIR@"
#cmakedefine INSTALL_PLUGINTESTDIR "@INSTALL_PLUGINTESTDIR@"
#cmakedefine INSTALL_INFODIR "@default_prefix@/@INSTALL_INFODIR@"
#cmakedefine INSTALL_MYSQLTESTDIR "@default_prefix@/@INSTALL_MYSQLTESTDIR@"
#cmakedefine INSTALL_DOCREADMEDIR "@default_prefix@/@INSTALL_DOCREADMEDIR@"
#cmakedefine INSTALL_DOCDIR "@default_prefix@/@INSTALL_DOCDIR@"
#cmakedefine INSTALL_MANDIR "@default_prefix@/@INSTALL_MANDIR@"
#cmakedefine INSTALL_SUPPORTFILESDIR "@default_prefix@/@INSTALL_SUPPORTFILESDIR@"
#cmakedefine INSTALL_LIBDIR "@default_prefix@/@INSTALL_LIBDIR@"

/*
 * Readline
 */
#cmakedefine HAVE_MBSTATE_T
#cmakedefine HAVE_LANGINFO_CODESET 
#cmakedefine HAVE_WCSDUP
#cmakedefine HAVE_WCHAR_T 1
#cmakedefine HAVE_WINT_T 1
#cmakedefine HAVE_CURSES_H 1
#cmakedefine HAVE_NCURSES_H 1
#cmakedefine USE_LIBEDIT_INTERFACE 1
#cmakedefine HAVE_HIST_ENTRY 1
#cmakedefine USE_NEW_EDITLINE_INTERFACE 1

/*
 * Libedit
 */
#cmakedefine HAVE_DECL_TGOTO 1

/*
 * DTrace
 */
#cmakedefine HAVE_DTRACE 1

/*
 * Character sets
 */
#cmakedefine MYSQL_DEFAULT_CHARSET_NAME "@MYSQL_DEFAULT_CHARSET_NAME@"
#cmakedefine MYSQL_DEFAULT_COLLATION_NAME "@MYSQL_DEFAULT_COLLATION_NAME@"
#cmakedefine HAVE_CHARSET_armscii8 1
#cmakedefine HAVE_CHARSET_ascii 1
#cmakedefine HAVE_CHARSET_big5 1
#cmakedefine HAVE_CHARSET_cp1250 1
#cmakedefine HAVE_CHARSET_cp1251 1
#cmakedefine HAVE_CHARSET_cp1256 1
#cmakedefine HAVE_CHARSET_cp1257 1
#cmakedefine HAVE_CHARSET_cp850 1
#cmakedefine HAVE_CHARSET_cp852 1 
#cmakedefine HAVE_CHARSET_cp866 1
#cmakedefine HAVE_CHARSET_cp932 1
#cmakedefine HAVE_CHARSET_dec8 1
#cmakedefine HAVE_CHARSET_eucjpms 1
#cmakedefine HAVE_CHARSET_euckr 1
#cmakedefine HAVE_CHARSET_gb2312 1
#cmakedefine HAVE_CHARSET_gbk 1
#cmakedefine HAVE_CHARSET_gb18030 1
#cmakedefine HAVE_CHARSET_geostd8 1
#cmakedefine HAVE_CHARSET_greek 1
#cmakedefine HAVE_CHARSET_hebrew 1
#cmakedefine HAVE_CHARSET_hp8 1
#cmakedefine HAVE_CHARSET_keybcs2 1
#cmakedefine HAVE_CHARSET_koi8r 1
#cmakedefine HAVE_CHARSET_koi8u 1
#cmakedefine HAVE_CHARSET_latin1 1
#cmakedefine HAVE_CHARSET_latin2 1
#cmakedefine HAVE_CHARSET_latin5 1
#cmakedefine HAVE_CHARSET_latin7 1
#cmakedefine HAVE_CHARSET_macce 1
#cmakedefine HAVE_CHARSET_macroman 1
#cmakedefine HAVE_CHARSET_sjis 1
#cmakedefine HAVE_CHARSET_swe7 1
#cmakedefine HAVE_CHARSET_tis620 1
#cmakedefine HAVE_CHARSET_ucs2 1
#cmakedefine HAVE_CHARSET_ujis 1
#cmakedefine HAVE_CHARSET_utf8mb4 1
#cmakedefine HAVE_CHARSET_utf8mb3 1
#cmakedefine HAVE_CHARSET_utf8 1
#cmakedefine HAVE_CHARSET_utf16 1
#cmakedefine HAVE_CHARSET_utf32 1
#cmakedefine HAVE_UCA_COLLATIONS 1

/*
 * Feature set
 */
#cmakedefine WITH_PARTITION_STORAGE_ENGINE 1

/*
 * Performance schema
 */
#cmakedefine WITH_PERFSCHEMA_STORAGE_ENGINE 1
#cmakedefine DISABLE_PSI_THREAD 1
#cmakedefine DISABLE_PSI_MUTEX 1
#cmakedefine DISABLE_PSI_RWLOCK 1
#cmakedefine DISABLE_PSI_COND 1
#cmakedefine DISABLE_PSI_FILE 1
#cmakedefine DISABLE_PSI_TABLE 1
#cmakedefine DISABLE_PSI_SOCKET 1
#cmakedefine DISABLE_PSI_STAGE 1
#cmakedefine DISABLE_PSI_STATEMENT 1
#cmakedefine DISABLE_PSI_SP 1
#cmakedefine DISABLE_PSI_PS 1
#cmakedefine DISABLE_PSI_IDLE 1
#cmakedefine DISABLE_PSI_STATEMENT_DIGEST 1
#cmakedefine DISABLE_PSI_METADATA 1
#cmakedefine DISABLE_PSI_MEMORY 1
#cmakedefine DISABLE_PSI_TRANSACTION 1

/*
 * syscall
*/
#cmakedefine HAVE_SYS_THREAD_SELFID 1
#cmakedefine HAVE_SYS_GETTID 1
#cmakedefine HAVE_PTHREAD_GETTHREADID_NP 1
#cmakedefine HAVE_PTHREAD_SETNAME_NP 1
#cmakedefine HAVE_INTEGER_PTHREAD_SELF 1

/* Platform-specific C++ compiler behaviors we rely upon */

/*
  This macro defines whether the compiler in use needs a 'typename' keyword
  to access the types defined inside a class template, such types are called
  dependent types. Some compilers require it, some others forbid it, and some
  others may work with or without it. For example, GCC requires the 'typename'
  keyword whenever needing to access a type inside a template, but msvc
  forbids it.
 */
#cmakedefine HAVE_IMPLICIT_DEPENDENT_NAME_TYPING 1


/*
 * MySQL version
 */
#cmakedefine DOT_FRM_VERSION @DOT_FRM_VERSION@
#define MYSQL_VERSION_MAJOR @MAJOR_VERSION@
#define MYSQL_VERSION_MINOR @MINOR_VERSION@
#define MYSQL_VERSION_PATCH @PATCH_VERSION@
#define MYSQL_VERSION_EXTRA "@EXTRA_VERSION@"
#define PACKAGE "mysql"
#define PACKAGE_BUGREPORT ""
#define PACKAGE_NAME "MySQL Server"
#define PACKAGE_STRING "MySQL Server @VERSION@"
#define PACKAGE_TARNAME "mysql"
#define PACKAGE_VERSION "@VERSION@"
#define VERSION "@VERSION@"
#define PROTOCOL_VERSION 10

/*
 * CPU info
 */
#cmakedefine CPU_LEVEL1_DCACHE_LINESIZE @CPU_LEVEL1_DCACHE_LINESIZE@

/*
 * NDB
 */
#cmakedefine WITH_NDBCLUSTER_STORAGE_ENGINE 1
#cmakedefine HAVE_PTHREAD_SETSCHEDPARAM 1

/*
 * Other
 */
#cmakedefine EXTRA_DEBUG 1
#cmakedefine HAVE_CHOWN 1

/*
 * Hardcoded values needed by libevent/NDB/memcached
 */
#define HAVE_FCNTL_H 1
#define HAVE_GETADDRINFO 1
#define HAVE_INTTYPES_H 1
/* libevent's select.c is not Windows compatible */
#ifndef _WIN32
#define HAVE_SELECT 1
#endif
#define HAVE_SIGNAL_H 1
#define HAVE_STDARG_H 1
#define HAVE_STDINT_H 1
#define HAVE_STDLIB_H 1
#define HAVE_STRDUP 1
#define HAVE_STRTOK_R 1
#define HAVE_STRTOLL 1
#define HAVE_SYS_STAT_H 1
#define HAVE_SYS_TYPES_H 1
#define SIZEOF_CHAR 1

/*
 * Needed by libevent
 */
#cmakedefine HAVE_SOCKLEN_T 1

/* For --secure-file-priv */
#cmakedefine DEFAULT_SECURE_FILE_PRIV_DIR @DEFAULT_SECURE_FILE_PRIV_DIR@
#cmakedefine DEFAULT_SECURE_FILE_PRIV_EMBEDDED_DIR @DEFAULT_SECURE_FILE_PRIV_EMBEDDED_DIR@
#cmakedefine HAVE_LIBNUMA 1

/* For default value of --early_plugin_load */
#cmakedefine DEFAULT_EARLY_PLUGIN_LOAD @DEFAULT_EARLY_PLUGIN_LOAD@

#endif
