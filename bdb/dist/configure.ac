# $Id: configure.ac,v 11.156 2002/09/04 13:51:17 bostic Exp $
# Process this file with autoconf to produce a configure script.

PACKAGE=db
AC_INIT(Berkeley DB,
	__EDIT_DB_VERSION__, support@sleepycat.com, db-__EDIT_DB_VERSION__)
AC_CONFIG_SRCDIR([../db/db.c])
AC_CONFIG_HEADER(db_config.h:config.hin)

# Configure setup.
AC_CANONICAL_HOST()
AC_ARG_PROGRAM()

# We cannot build in the top-level directory.
AC_MSG_CHECKING(if building in the top-level directory)
[ test -d db_archive ] && AC_MSG_ERROR([
Berkeley DB cannot be built in the top-level distribution directory.])
AC_MSG_RESULT(no)

# Minimum autoconf version required.
AC_PREREQ(2.53)

# Substitution variables.
AC_SUBST(ADDITIONAL_INCS)
AC_SUBST(ADDITIONAL_LANG)
AC_SUBST(ADDITIONAL_OBJS)
AC_SUBST(ADDITIONAL_PROGS)
AC_SUBST(BUILD_TARGET)
AC_SUBST(CFLAGS)
AC_SUBST(CONFIGURATION_ARGS)
AC_SUBST(CONFIGURATION_PATH)
AC_SUBST(CPPFLAGS)
AC_SUBST(CXX)
AC_SUBST(CXXFLAGS)
AC_SUBST(DEFAULT_LIB)
AC_SUBST(DEFAULT_LIB_CXX)
AC_SUBST(EMBEDIX_ECD_CXX)
AC_SUBST(EMBEDIX_ECD_RPC)
AC_SUBST(EMBEDIX_ROOT)
AC_SUBST(INSTALLER)
AC_SUBST(INSTALL_LIBS)
AC_SUBST(INSTALL_TARGET)
AC_SUBST(JAR)
AC_SUBST(JAVACFLAGS)
AC_SUBST(LDFLAGS)
AC_SUBST(LIBJSO_LIBS)
AC_SUBST(LIBS)
AC_SUBST(LIBSO_LIBS)
AC_SUBST(LIBTOOL)
AC_SUBST(LIBTSO_LIBS)
AC_SUBST(LIBXSO_LIBS)
AC_SUBST(LOAD_LIBS)
AC_SUBST(MAKEFILE_CC)
AC_SUBST(MAKEFILE_CCLINK)
AC_SUBST(MAKEFILE_CXX)
AC_SUBST(MAKEFILE_CXXLINK)
AC_SUBST(MAKEFILE_SOLINK)
AC_SUBST(MAKEFILE_XSOLINK)
AC_SUBST(POSTLINK)
AC_SUBST(RPC_CLIENT_OBJS)
AC_SUBST(RPM_POST_INSTALL)
AC_SUBST(RPM_POST_UNINSTALL)
AC_SUBST(SOFLAGS)
AC_SUBST(db_cv_path_embedix_install)
AC_SUBST(db_cv_path_rpm_archive)
AC_SUBST(db_int_def)
AC_SUBST(o)

# RPM needs the current absolute path.
# RPM needs the list of original arguments, but we don't include the RPM
# option itself.
CONFIGURATION_PATH=${PWD-`pwd`}
CONFIGURATION_ARGS=`echo "$*" |
    sed -e 's/--with-embedix[[^ ]]*//' -e 's/--with-rpm[[^ ]]*//'`

# Set the version.
AM_VERSION_SET

# Set the default installation location.
AC_PREFIX_DEFAULT(/usr/local/BerkeleyDB.@DB_VERSION_MAJOR@.@DB_VERSION_MINOR@)

# Process all options before using them.
AM_OPTIONS_SET

# Set some #defines based on configuration options.
if test "$db_cv_diagnostic" = yes; then
	AC_DEFINE(DIAGNOSTIC)
	AH_TEMPLATE(DIAGNOSTIC,
    [Define to 1 if you want a version with run-time diagnostic checking.])
fi
if test "$db_cv_debug_rop" = yes; then
	AC_DEFINE(DEBUG_ROP)
	AH_TEMPLATE(DEBUG_ROP,
    [Define to 1 if you want a version that logs read operations.])
fi
if test "$db_cv_debug_wop" = yes; then
	AC_DEFINE(DEBUG_WOP)
	AH_TEMPLATE(DEBUG_WOP,
    [Define to 1 if you want a version that logs write operations.])
fi
if test "$db_cv_umrw" = yes; then
	AC_DEFINE(UMRW)
	AH_TEMPLATE(UMRW,
    [Define to 1 to mask harmless unitialized memory read/writes.])

fi
if test "$db_cv_test" = yes; then
	AC_DEFINE(CONFIG_TEST)
	AH_TEMPLATE(CONFIG_TEST,
    [Define to 1 if you want to build a version for running the test suite.])
fi

# Check for programs used in building and installation.
AM_PROGRAMS_SET
AC_PROG_INSTALL

# RPM/Embedix support: change the standard make and install targets
if test "$db_cv_rpm" = "yes"; then
	BUILD_TARGET="rpm_build"
	echo "topdir: $CONFIGURATION_PATH" > rpmrc
	if test "$db_cv_embedix" = "yes"; then
		EMBEDIX_ROOT="/usr"
		INSTALL_TARGET="embedix_install"
	else
		INSTALL_TARGET="rpm_install"
	fi
else
	BUILD_TARGET="library_build"
	INSTALL_TARGET="library_install"
fi

# This is where we handle stuff that autoconf can't handle: compiler,
# preprocessor and load flags, libraries that the standard tests don't
# look for.  The default optimization is -O.  We would like to set the
# default optimization for systems using gcc to -O2, but we can't.  By
# the time we know we're using gcc, it's too late to set optimization
# flags.
#
# There are additional libraries we need for some compiler/architecture
# combinations.
#
# Some architectures require DB to be compiled with special flags and/or
# libraries for threaded applications
#
# The makefile CC may be different than the CC used in config testing,
# because the makefile CC may be set to use $(LIBTOOL).
#
# XXX
# Don't override anything if it's already set from the environment.
optimize_def="-O"
case "$host_os" in
aix4.3.*|aix5*)
	optimize_def="-O2"
	CC=${CC-"xlc_r"}
	CPPFLAGS="$CPPFLAGS -D_THREAD_SAFE"
	LDFLAGS="$LDFLAGS -Wl,-brtl";;
bsdi3*)	optimize_def="-O2"
	CC=${CC-"shlicc2"}
	LIBS="$LIBS -lipc";;
bsdi*)	optimize_def="-O2";;
freebsd*)
	optimize_def="-O2"
	CPPFLAGS="$CPPFLAGS -D_THREAD_SAFE"
	LDFLAGS="$LDFLAGS -pthread";;
hpux*)	CPPFLAGS="$CPPFLAGS -D_REENTRANT";;
irix*)	optimize_def="-O2"
	CPPFLAGS="$CPPFLAGS -D_SGI_MP_SOURCE";;
linux*)	optimize_def="-O2"
	CPPFLAGS="$CPPFLAGS -D_GNU_SOURCE -D_REENTRANT";;
mpeix*)	CPPFLAGS="$CPPFLAGS -D_POSIX_SOURCE -D_SOCKET_SOURCE"
	LIBS="$LIBS -lsocket -lsvipc";;
osf*)	CPPFLAGS="$CPPFLAGS -D_REENTRANT"
	LDFLAGS="$LDFLAGS -pthread";;
*qnx)	AC_DEFINE(HAVE_QNX)
	AH_TEMPLATE(HAVE_QNX, [Define to 1 if building on QNX.]);;
solaris*)
	CPPFLAGS="$CPPFLAGS -D_REENTRANT";;
esac

# Set CFLAGS/CXXFLAGS.  We MUST set the flags before we call autoconf
# compiler configuration macros, because if we don't, they set CFLAGS
# to no optimization and -g, which isn't what we want.
CFLAGS=${CFLAGS-$optimize_def}
CXXFLAGS=${CXXFLAGS-"$CFLAGS"}

# If the user wants a debugging environment, add -g to the CFLAGS value.
#
# XXX
# Some compilers can't mix optimizing and debug flags.  The only way to
# handle this is to specify CFLAGS in the environment before configuring.
if test "$db_cv_debug" = yes; then
	AC_DEFINE(DEBUG)
	AH_TEMPLATE(DEBUG, [Define to 1 if you want a debugging version.])

	CFLAGS="$CFLAGS -g"
	CXXFLAGS="$CXXFLAGS -g"
fi

# The default compiler is cc (NOT gcc), the default CFLAGS is as specified
# above, NOT what is set by AC_PROG_CC, as it won't set optimization flags
# for any compiler other than gcc.
AC_PROG_CC(cc gcc)

# Because of shared library building, the ${CC} used for config tests
# may be different than the ${CC} we want to put in the Makefile.
# The latter is known as ${MAKEFILE_CC} in this script.
MAKEFILE_CC="${CC}"
MAKEFILE_CCLINK="${CC}"
MAKEFILE_CXX="nocxx"
MAKEFILE_CXXLINK="nocxx"

# See if we need the C++ compiler at all.  If so, we'd like to find one that
# interoperates with the C compiler we chose.  Since we prefered cc over gcc,
# we'll also prefer the vendor's compiler over g++/gcc.  If we're wrong, the
# user can set CC and CXX in their environment before running configure.
#
# AC_PROG_CXX sets CXX, but it uses $CXX and $CCC (in that order) as its
# first choices.
if test "$db_cv_cxx" = "yes"; then
	if test "$GCC" != "yes"; then
		case "$host_os" in
		aix*)		AC_CHECK_TOOL(CCC, xlC_r)
				LIBXSO_LIBS="-lC_r $LIBXSO_LIBS"
				LIBS="-lC_r $LIBS";;
		hpux*)		AC_CHECK_TOOL(CCC, aCC);;
		irix*)		AC_CHECK_TOOL(CCC, CC);;
		osf*)		AC_CHECK_TOOL(CCC, cxx);;
		solaris*)	AC_CHECK_TOOL(CCC, CC);;
		esac
	fi
	AC_PROG_CXX
	AC_CXX_HAVE_STDHEADERS
	MAKEFILE_CXX="${CXX}"
	MAKEFILE_CXXLINK="${CXX}"
fi

# Do some gcc specific configuration.
AC_GCC_CONFIG1
AC_GCC_CONFIG2

# We need the -Kthread/-pthread flag when compiling on SCO/Caldera's UnixWare
# and OpenUNIX releases.  We can't make the test until we know which compiler
# we're using.
case "$host_os" in
sysv5UnixWare*|sysv5OpenUNIX8*)
	if test "$GCC" == "yes"; then
		CPPFLAGS="$CPPFLAGS -pthread"
		LDFLAGS="$LDFLAGS -pthread"
	else
		CPPFLAGS="$CPPFLAGS -Kthread"
		LDFLAGS="$LDFLAGS -Kthread"
	fi;;
esac

# Export our compiler preferences for the libtool configuration.
export CC CCC
CCC=CXX

# Libtool configuration.
AC_PROG_LIBTOOL

LIBTOOL="\$(SHELL) ./libtool"
SOFLAGS="-rpath \$(libdir)"

# Set SOSUFFIX and friends
SOSUFFIX_CONFIG
MODSUFFIX_CONFIG
JMODSUFFIX_CONFIG

INSTALLER="\$(LIBTOOL) --mode=install cp -p"

MAKEFILE_CC="\$(LIBTOOL) --mode=compile ${MAKEFILE_CC}"
MAKEFILE_SOLINK="\$(LIBTOOL) --mode=link ${MAKEFILE_CCLINK} -avoid-version"
MAKEFILE_CCLINK="\$(LIBTOOL) --mode=link ${MAKEFILE_CCLINK}"
MAKEFILE_CXX="\$(LIBTOOL) --mode=compile ${MAKEFILE_CXX}"
MAKEFILE_XSOLINK="\$(LIBTOOL) --mode=link ${MAKEFILE_CXXLINK} -avoid-version"
MAKEFILE_CXXLINK="\$(LIBTOOL) --mode=link ${MAKEFILE_CXXLINK}"

# Configure for shared libraries, static libraries, or both.  If both are
# configured, build the utilities and example programs with shared versions.
#
# $o is set to ".o" or ".lo", and is the file suffix used in the Makefile
# instead of .o
if test "$enable_shared" = "no"; then
	DEFAULT_LIB="\$(libdb)"
	POSTLINK="@true"
	o=".o"
fi
if test "$enable_shared" = "yes"; then
	DEFAULT_LIB="\$(libso_target)"
	POSTLINK="\$(LIBTOOL) --mode=execute true"
	o=".lo"
fi
INSTALL_LIBS="$DEFAULT_LIB"

# Optional C++ API.
if test "$db_cv_cxx" = "yes"; then
	if test "$enable_shared" = "no"; then
		DEFAULT_LIB_CXX="\$(libcxx)"
	fi
	if test "$enable_shared" = "yes"; then
		DEFAULT_LIB_CXX="\$(libxso_target)"
	fi
	INSTALL_LIBS="$INSTALL_LIBS $DEFAULT_LIB_CXX"

	# Fill in C++ library for Embedix.
	EMBEDIX_ECD_CXX='<OPTION db-extra>\
	TYPE=bool\
	DEFAULT_VALUE=1\
	PROMPT=Include BerkeleyDB C++ library?\
	<KEEPLIST>\
		/usr/include/db_cxx.h\
		/usr/lib/libdb_cxx-@DB_VERSION_MAJOR@.@DB_VERSION_MINOR@.so\
	</KEEPLIST>\
	<PROVIDES>\
		libdb_cxx-@DB_VERSION_MAJOR@.@DB_VERSION_MINOR@.so\
	</PROVIDES>\
	<REQUIRES>\
		ld-linux.so.2\
		libc.so.6\
	</REQUIRES>\
	STATIC_SIZE=0\
	STORAGE_SIZE=523612\
	STARTUP_TIME=0\
	</OPTION>'
fi

# Optional Java API.
if test "$db_cv_java" = "yes"; then
	# Java requires shared libraries.
	if test "$enable_shared" = "no"; then
		AC_MSG_ERROR([Java requires shared libraries])
	fi

	AC_PROG_JAVAC
	AC_PROG_JAR
        AC_JNI_INCLUDE_DIR

	for JNI_INCLUDE_DIR in $JNI_INCLUDE_DIRS
	do
		CPPFLAGS="$CPPFLAGS -I$JNI_INCLUDE_DIR"
	done

	ADDITIONAL_LANG="$ADDITIONAL_LANG java"
	INSTALL_LIBS="$INSTALL_LIBS \$(libjso_target)"
else
	JAVAC=nojavac
fi

# Optional RPC client/server.
if test "$db_cv_rpc" = "yes"; then
	AC_DEFINE(HAVE_RPC)
	AH_TEMPLATE(HAVE_RPC, [Define to 1 if building RPC client/server.])

	RPC_CLIENT_OBJS="\$(RPC_CLIENT_OBJS)"
	ADDITIONAL_PROGS="berkeley_db_svc $ADDITIONAL_PROGS"
	
	EMBEDIX_ECD_RPC="/usr/bin/berkeley_db_svc"

	case "$host_os" in
	hpux*)
		AC_CHECK_FUNC(svc_run,,
		    AC_CHECK_LIB(nsl, svc_run,
		    LIBS="-lnsl $LIBS"; LIBTSO_LIBS="-lnsl $LIBTSO_LIBS"));;
	solaris*)
		AC_CHECK_FUNC(svc_run,, AC_CHECK_LIB(nsl, svc_run));;
	esac
fi

AM_TCL_LOAD

# Optional crypto support.
if test -d "$srcdir/../crypto"; then
	AC_DEFINE(HAVE_CRYPTO)
	AH_TEMPLATE(HAVE_CRYPTO,
    [Define to 1 if Berkeley DB release includes strong cryptography.])
	ADDITIONAL_OBJS="aes_method${o} crypto${o} mt19937db${o} rijndael-alg-fst${o} rijndael-api-fst${o} $ADDITIONAL_OBJS"
fi

# Optional DB 1.85 compatibility API.
if test "$db_cv_compat185" = "yes"; then
	ADDITIONAL_INCS="db_185.h $ADDITIONAL_INCS"
	ADDITIONAL_OBJS="db185${o} $ADDITIONAL_OBJS"
fi

# Optional utilities.
if test "$db_cv_dump185" = "yes"; then
	ADDITIONAL_PROGS="db_dump185 $ADDITIONAL_PROGS"
fi

# Checks for compiler characteristics.
AC_C_CONST

# Checks for include files, structures, C types.
AC_HEADER_STAT
AC_HEADER_TIME
AC_HEADER_DIRENT
AC_CHECK_HEADERS(sys/select.h sys/time.h)
AC_CHECK_MEMBERS([struct stat.st_blksize])
AM_TYPES

AC_CACHE_CHECK([for ANSI C exit success/failure values], db_cv_exit_defines, [
AC_TRY_COMPILE([#include <stdlib.h>], return (EXIT_SUCCESS);,
    [db_cv_exit_defines=yes], [db_cv_exit_defines=no])])
if test "$db_cv_exit_defines" = yes; then
	AC_DEFINE(HAVE_EXIT_SUCCESS)
	AH_TEMPLATE(HAVE_EXIT_SUCCESS,
    [Define to 1 if you have EXIT_SUCCESS/EXIT_FAILURE #defines.])
fi

# Test for various functions/libraries that the test and example programs use:
#	sched_yield function
#	pthreads, socket and math libraries
AC_CHECK_FUNC(sched_yield,,
    AC_SEARCH_LIBS(sched_yield, rt, LOAD_LIBS="$LOAD_LIBS -lrt"))

# XXX
# We can't check for pthreads in the same way we did the test for sched_yield
# because the Solaris C library includes pthread interfaces which are not
# thread-safe.  For that reason we always add -lpthread if we find a pthread
# library.  Also we can't depend on any specific call existing (pthread_create,
# for example), as it may be #defined in an include file -- OSF/1 (Tru64) has
# this problem.
AC_HAVE_LIBRARY(pthread, LOAD_LIBS="$LOAD_LIBS -lpthread")

# XXX
# We could be more exact about whether these libraries are needed, but we don't
# bother -- if they exist, we load them.
AC_HAVE_LIBRARY(m, LOAD_LIBS="$LOAD_LIBS -lm")
AC_HAVE_LIBRARY(socket, LOAD_LIBS="$LOAD_LIBS -lsocket")
AC_HAVE_LIBRARY(nsl, LOAD_LIBS="$LOAD_LIBS -lnsl")

# Check for mutexes.
# We do this here because it changes $LIBS.
AM_DEFINE_MUTEXES

# Checks for system functions for which we have replacements.
#
# XXX
# The only portable getcwd call is getcwd(char *, size_t), where the
# buffer is non-NULL -- Solaris can't handle a NULL buffer, and they
# deleted getwd().
AC_REPLACE_FUNCS(getcwd getopt memcmp memcpy memmove raise)
AC_REPLACE_FUNCS(snprintf strcasecmp strdup strerror vsnprintf)

# Check for system functions we optionally use.
AC_CHECK_FUNCS(_fstati64 clock_gettime directio gettimeofday getuid)
AC_CHECK_FUNCS(pstat_getdynamic sched_yield select strtoul sysconf yield)

# Checks for system functions for which we don't have replacements.
# We require qsort(3).
AC_CHECK_FUNCS(qsort, , AC_MSG_ERROR([No qsort library function.]))

# Pread/pwrite.
# HP-UX has pread/pwrite, but it doesn't work with largefile support.
case "$host_os" in
hpux*)
	AC_MSG_WARN([pread/pwrite interfaces ignored on $host_os.]);;
*)	AC_CHECK_FUNCS(pread pwrite)
esac

# Check for fcntl(2) to deny child process access to file descriptors.
AC_CACHE_CHECK([for fcntl/F_SETFD], db_cv_fcntl_f_setfd, [
AC_TRY_LINK([
#include <sys/types.h>
#include <fcntl.h>], [
	fcntl(1, F_SETFD, 1);
], [db_cv_fcntl_f_setfd=yes], [db_cv_fcntl_f_setfd=no])])
if test "$db_cv_fcntl_f_setfd" = yes; then
	AC_DEFINE(HAVE_FCNTL_F_SETFD)
	AH_TEMPLATE(HAVE_FCNTL_F_SETFD,
    [Define to 1 if fcntl/F_SETFD denies child access to file descriptors.])
fi

# A/UX has a broken getopt(3).
case "$host_os" in
aux*)	ADDITIONAL_OBJS="getopt${o} $ADDITIONAL_OBJS";;
esac

# Linux has the O_DIRECT flag, but you can't actually use it.
AC_CACHE_CHECK([for open/O_DIRECT], db_cv_open_o_direct, [
echo "test for working open/O_DIRECT" > __o_direct_file
AC_TRY_RUN([
#include <sys/types.h>
#include <fcntl.h>
main() {
int c, fd = open("__o_direct_file", O_RDONLY | O_DIRECT, 0);
exit ((fd == -1) || (read(fd, &c, 1) != 1));
}], [db_cv_open_o_direct=yes], [db_cv_open_o_direct=no],
AC_TRY_LINK([
#include <sys/types.h>   
#include <fcntl.h>], [
	open("__o_direct_file", O_RDONLY | O_DIRECT, 0);
], [db_cv_open_o_direct=yes], [db_cv_open_o_direct=no]))
rm -f __o_direct_file])
if test "$db_cv_open_o_direct" = yes; then
	AC_DEFINE(HAVE_O_DIRECT)
	AH_TEMPLATE(HAVE_O_DIRECT, [Define to 1 if you have the O_DIRECT flag.])
fi

# Check for largefile support.
AC_SYS_LARGEFILE

# Figure out how to create shared regions.
#
# First, we look for mmap.
#
# BSD/OS has mlock(2), but it doesn't work until the 4.1 release.
#
# Nextstep (version 3.3) apparently supports mmap(2) (the mmap symbol
# is defined in the C library) but does not support munmap(2).  Don't
# try to use mmap if we can't find munmap.
#
# Ultrix has mmap(2), but it doesn't work.
mmap_ok=no
case "$host_os" in
bsdi3*|bsdi4.0)
	AC_MSG_WARN([mlock(2) interface ignored on BSD/OS 3.X and 4.0.])
	mmap_ok=yes
	AC_CHECK_FUNCS(mmap munmap, , mmap_ok=no);;
ultrix*)
	AC_MSG_WARN([mmap(2) interface ignored on Ultrix.]);;
*)
	mmap_ok=yes
	AC_CHECK_FUNCS(mlock munlock)
	AC_CHECK_FUNCS(mmap munmap, , mmap_ok=no);;
esac

# Second, we look for shmget.
#
# SunOS has the shmget(2) interfaces, but there appears to be a missing
# #include <debug/debug.h> file, so we ignore them.
shmget_ok=no
case "$host_os" in
sunos*)
	AC_MSG_WARN([shmget(2) interface ignored on SunOS.]);;
*)
	shmget_ok=yes
	AC_CHECK_FUNCS(shmget, , shmget_ok=no);;
esac

# We require either mmap/munmap(2) or shmget(2).
if test "$mmap_ok" = no -a "$shmget_ok" = no; then
	AC_MSG_WARN([Neither mmap/munmap(2) or shmget(2) library functions.])
fi

# If we're not doing version name substitution, DB_VERSION_UNIQUE_NAME
# needs to be erased.
if test "$db_cv_uniquename" = "no"; then
	DB_VERSION_UNIQUE_NAME=""
fi

# This is necessary so that .o files in LIBOBJS are also built via
# the ANSI2KNR-filtering rules.
LIB@&t@OBJS=`echo "$LIB@&t@OBJS" |
	     sed 's,\.[[^.]]* ,$U&,g;s,\.[[^.]]*$,$U&,'`
LTLIBOBJS=`echo "$LIB@&t@OBJS" |
	   sed "s,\.[[^.]]* ,$o ,g;s,\.[[^.]]*$,$o,"`
AC_SUBST(LTLIBOBJS)

# Initial output file list.
CREATE_LIST="Makefile
    db_cxx.h:$srcdir/../dbinc/db_cxx.in
    db_int.h:$srcdir/../dbinc/db_int.in
    include.tcl:$srcdir/../test/include.tcl"

# Create the db.h file from a source file, a list of global function
# prototypes, and, if configured for unique names, a list of #defines
# to do DB_VERSION_UNIQUE_NAME substitution.
if test "$db_cv_uniquename" = "yes"; then
	CREATE_LIST="$CREATE_LIST
	db.h:$srcdir/../dbinc/db.in:$srcdir/../dbinc_auto/rpc_defs.in:$srcdir/../dbinc_auto/ext_def.in:$srcdir/../dbinc_auto/ext_prot.in"
else
	CREATE_LIST="$CREATE_LIST
	db.h:$srcdir/../dbinc/db.in:$srcdir/../dbinc_auto/rpc_defs.in:$srcdir/../dbinc_auto/ext_prot.in"
fi

# If configured for unique names, create the db_int_uext.h file (which
# does the DB_VERSION_UNIQUE_NAME substitution), which is included by
# the db_int.h file.
if test "$db_cv_uniquename" = "yes"; then
	CREATE_LIST="$CREATE_LIST
	db_int_def.h:$srcdir/../dbinc_auto/int_def.in"
	db_int_def='#include "db_int_def.h"'
fi

# Create the db_185.h and db185_int.h files from source files, a list of
# global function prototypes, and, if configured for unique names, a list
# of #defines to do DB_VERSION_UNIQUE_NAME substitution.
if test "$db_cv_compat185" = "yes"; then
	if test "$db_cv_uniquename" = "yes"; then
		CREATE_LIST="$CREATE_LIST
		db_185.h:$srcdir/../dbinc/db_185.in:$srcdir/../dbinc_auto/ext_185_def.in:$srcdir/../dbinc_auto/ext_185_prot.in
		db185_int.h:$srcdir/../db185/db185_int.in:$srcdir/../dbinc_auto/ext_185_def.in:$srcdir/../dbinc_auto/ext_185_prot.in"
	else
		CREATE_LIST="$CREATE_LIST
		db_185.h:$srcdir/../dbinc/db_185.in:$srcdir/../dbinc_auto/ext_185_prot.in
		db185_int.h:$srcdir/../db185/db185_int.in:$srcdir/../dbinc_auto/ext_185_prot.in"
	fi
fi

if test "$db_cv_embedix" = "yes"; then
	CREATE_LIST="$CREATE_LIST db.ecd:../dist/db.ecd.in"
fi

if test "$db_cv_rpm" = "yes"; then
	CREATE_LIST="$CREATE_LIST db.spec:../dist/db.spec.in"
fi

AC_CONFIG_FILES($CREATE_LIST)
AC_OUTPUT
