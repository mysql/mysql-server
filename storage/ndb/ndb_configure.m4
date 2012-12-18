# The NDB version number and status.
# Should be updated when creating a new NDB version
NDB_VERSION_MAJOR=7
NDB_VERSION_MINOR=0
NDB_VERSION_BUILD=38
NDB_VERSION_STATUS=""

dnl for build ndb docs

AC_PATH_PROG(DOXYGEN, doxygen, no)
AC_PATH_PROG(PDFLATEX, pdflatex, no)
AC_PATH_PROG(MAKEINDEX, makeindex, no)

AC_SUBST(DOXYGEN)
AC_SUBST(PDFLATEX)
AC_SUBST(MAKEINDEX)

dnl ---------------------------------------------------------------------------
dnl Check if ndbmtd should/can be built
dnl - skipped if with --without-ndbmtd specified
dnl - skipped if the ndbmtd assembler can't be compiled
dnl
dnl ---------------------------------------------------------------------------
# Dummy define of BUILD_NDBMTD to satisfy builds without ndb
AM_CONDITIONAL([BUILD_NDBMTD], [ false ])
AC_DEFUN([NDB_CHECK_NDBMTD], [

  build_ndbmtd=

  AC_ARG_WITH([ndbmtd],
              [AC_HELP_STRING([--without-ndbmtd],
                              [Dont build ndbmtd])],
              [ndb_ndbmtd="$withval"],
              [ndb_ndbmtd=yes])

  if test X"$ndb_ndbmtd" = Xyes
  then
    # checking atomic.h needed for spinlock's on sparc and Sun Studio
    AC_CHECK_HEADERS(atomic.h)

    # checking assembler needed for ndbmtd
    SAVE_CFLAGS="$CFLAGS"
    if test "x${ac_cv_header_atomic_h}" = xyes; then
      CFLAGS="$CFLAGS -DHAVE_ATOMIC_H"
    fi
    AC_CACHE_CHECK([assembler needed for ndbmtd],
                   [ndb_cv_ndbmtd_asm],[
      AC_TRY_RUN(
        [
        #include "storage/ndb/src/kernel/vm/mt-asm.h"
        #ifdef NDB_NO_ASM
        #error "compiler/arch does not have asm needed for ndbmtd"
        #endif
        int main()
        {
          unsigned int a = 0;
          volatile unsigned int *ap = (volatile unsigned int*)&a;
        #ifdef NDB_HAVE_XCNG
          a = xcng(ap, 1);
          cpu_pause();
        #endif
          mb();
          * ap = 2;
          rmb();
          * ap = 1;
          wmb();
          * ap = 0;
          read_barrier_depends();
          return a;
        }
        ],
        [ndb_cv_ndbmtd_asm=yes],
        [ndb_cv_ndbmtd_asm=no],
        [ndb_cv_ndbmtd_asm=no]
      )]
    )
    CFLAGS="$SAVE_CFLAGS"

    if test X"$ndb_cv_ndbmtd_asm" = Xyes
    then
      build_ndbmtd=yes
      AC_MSG_RESULT([Including ndbmtd])
    fi
  fi

  # Redefine BUILD_NDBMTD now when result is known(otherwise the test
  # is evaluated too early in configure)
  AM_CONDITIONAL([BUILD_NDBMTD], [ test X"$build_ndbmtd" = Xyes ])

])


AC_DEFUN([MYSQL_CHECK_NDB_JTIE], [

case "$host_os" in
darwin*)        INC="Headers";;
*)              INC="include";;
esac

dnl
dnl Search for JAVA_HOME
dnl

for D in $JAVA_HOME $JDK_HOME /usr/lib/jvm/java /usr/lib64/jvm/java /usr/local/jdk /usr/local/java /System/Library/Frameworks/JavaVM.framework/Versions/CurrentJDK ; do
        AC_CHECK_FILE([$D/$INC/jni.h],[found=yes])
        if test X$found = Xyes
        then
                JINC=$D/$INC
                break;
        fi
done

if test -f "${JINC}/jni.h"
then
        JNI_INCLUDE_DIRS="-I${JINC}"
else
        AC_MSG_RESULT([-- Unable to locate jni.h!])
fi

dnl try to add extra include path
case "$host_os" in
bsdi*)    JNI_SUBDIRS="bsdos";;
linux*)   JNI_SUBDIRS="linux genunix";;
osf*)     JNI_SUBDIRS="alpha";;
solaris*) JNI_SUBDIRS="solaris";;
mingw*)   JNI_SUBDIRS="win32";;
cygwin*)  JNI_SUBDIRS="win32";;
*)        JNI_SUBDIRS="genunix";;
esac

dnl add any subdirectories that are present
for S in ${JNI_SUBDIRS}
do
        if test -d "${JINC}/${S}"
        then
                JNI_INCLUDE_DIRS="${JNI_INCLUDE_DIRS} -I${JINC}/${S}"
        fi
done

CPPFLAGS_save="$CPPFLAGS"
CPPFLAGS="$CPPFLAGS ${JNI_INCLUDE_DIRS}"
AC_CHECK_HEADERS(jni.h)
CPPFLAGS="$CPPFLAGS_save"

AC_CHECK_PROG(JAVAC, javac, javac, no)
AC_CHECK_PROG(JAVAH, javah, javah, no)
AC_CHECK_PROG(JAR, jar, jar, no)
AC_SUBST(JNI_INCLUDE_DIRS)

ndb_jtie_supported=no
if test "$JAVAC" &&
   test "$JAVAH" &&
   test "$JAR" &&
   test X"$ac_cv_header_jni_h" = Xyes
then
        ndb_jtie_supported=yes
fi
])

AC_DEFUN([NDB_COMPILER_FEATURES],
[
  AC_LANG_PUSH([C++])
  AC_MSG_CHECKING([checking __is_pod(typename)])
  AC_TRY_COMPILE([struct A{};],[ int a = __is_pod(A)],
    [ AC_MSG_RESULT([yes])
      AC_DEFINE([HAVE___IS_POD], [1],
              [Compiler supports __is_pod(typename)])],
    AC_MSG_RESULT([no])
  )

  AC_MSG_CHECKING([checking __has_trivial_constructor(typename)])
  AC_TRY_COMPILE([struct A{};], [ int a = __has_trivial_constructor(A)],
    [ AC_MSG_RESULT([yes])
      AC_DEFINE([HAVE___HAS_TRIVIAL_CONSTRUCTOR], [1],
              [Compiler supports __has_trivial_constructor(typename)])],
    AC_MSG_RESULT([no])
  )

  # need c++ here, cause c will accept function wo/ prototype
  # which will later lead to link error
  AC_MSG_CHECKING([checking __builtin_ffs(unsigned)])
  AC_TRY_COMPILE([unsigned A = 7;],[ unsigned a = __builtin_ffs(A)],
    [ AC_MSG_RESULT([yes])
      AC_DEFINE([HAVE___BUILTIN_FFS], [1],
              [Compiler supports __builtin_ffs])],
    AC_MSG_RESULT([no])
  )

  AC_MSG_CHECKING([checking __builtin_ctz(unsigned)])
  AC_TRY_COMPILE([unsigned A = 7;],[ unsigned a = __builtin_ctz(A)],
    [ AC_MSG_RESULT([yes])
      AC_DEFINE([HAVE___BUILTIN_CTZ], [1],
              [Compiler supports __builtin_ctz])],
    AC_MSG_RESULT([no])
  )

  AC_MSG_CHECKING([checking __builtin_clz(unsigned)])
  AC_TRY_COMPILE([unsigned A = 7;],[ unsigned a = __builtin_clz(A)],
    [ AC_MSG_RESULT([yes])
      AC_DEFINE([HAVE___BUILTIN_CLZ], [1],
              [Compiler supports __builtin_clz])],
    AC_MSG_RESULT([no])
  )

  AC_LANG_POP([C++])
])

AC_DEFUN([MYSQL_CHECK_NDB_OPTIONS], [
  AC_ARG_WITH([ndb-sci],
              AC_HELP_STRING([--with-ndb-sci=DIR],
                             [Provide MySQL with a custom location of
                             sci library. Given DIR, sci library is 
                             assumed to be in $DIR/lib and header files
                             in $DIR/include.]),
              [mysql_sci_dir=${withval}],
              [mysql_sci_dir=""])

  case "$mysql_sci_dir" in
    "no" )
      have_ndb_sci=no
      AC_MSG_RESULT([-- not including sci transporter])
      ;;
    * )
      if test -f "$mysql_sci_dir/lib/libsisci.a" -a \ 
              -f "$mysql_sci_dir/include/sisci_api.h"; then
        NDB_SCI_INCLUDES="-I$mysql_sci_dir/include"
        NDB_SCI_LIBS="$mysql_sci_dir/lib/libsisci.a"
        AC_MSG_RESULT([-- including sci transporter])
        AC_DEFINE([NDB_SCI_TRANSPORTER], [1],
                  [Including Ndb Cluster DB sci transporter])
        AC_SUBST(NDB_SCI_INCLUDES)
        AC_SUBST(NDB_SCI_LIBS)
        have_ndb_sci="yes"
        AC_MSG_RESULT([found sci transporter in $mysql_sci_dir/{include, lib}])
      else
        AC_MSG_RESULT([could not find sci transporter in $mysql_sci_dir/{include, lib}])
      fi
      ;;
  esac

  AC_ARG_WITH([ndb-test],
              [AC_HELP_STRING([--with-ndb-test],
                              [Include the NDB Cluster ndbapi test programs])],
              [ndb_test="$withval"],
              [ndb_test=no])
  AC_ARG_WITH([ndb-docs],
              [AC_HELP_STRING([--with-ndb-docs],
              [Include the NDB Cluster ndbapi and mgmapi documentation])],
              [ndb_docs="$withval"],
              [ndb_docs=no])
  AC_ARG_WITH([ndb-port],
              [AC_HELP_STRING([--with-ndb-port=port-number],
              [Default port used by NDB Cluster management server])],
              [ndb_port="$withval"],[ndb_port="no"])
  case "$ndb_port" in
    "yes" )
      AC_MSG_ERROR([--with-ndb-port=<port-number> needs an argument])
      ;;
    "no" )
      ;;
    * )
      AC_DEFINE_UNQUOTED([NDB_PORT], [$ndb_port],
                         [Default port used by NDB Cluster management server])
      ;;
  esac

  AC_ARG_WITH([ndb-port-base],
              [AC_HELP_STRING([--with-ndb-port-base],
                              [Deprecated option])],
              [ndb_port_base="$withval"], [])
  if test "$ndb_port_base"
  then
     AC_MSG_WARN([Ignoring deprecated option --with-ndb-port-base])
  fi

  AC_ARG_WITH([ndb-debug],
              [AC_HELP_STRING([--without-ndb-debug],
                              [Disable special ndb debug features])],
              [ndb_debug="$withval"],
              [ndb_debug="default"])
  AC_ARG_WITH([ndb-ccflags],
              [AC_HELP_STRING([--with-ndb-ccflags=CFLAGS],
                              [Extra CFLAGS for ndb compile])],
              [ndb_ccflags=${withval}],
              [ndb_ccflags=""])
  AC_ARG_WITH([ndb-binlog],
              [AC_HELP_STRING([--without-ndb-binlog],
                              [Disable ndb binlog])],
              [ndb_binlog="$withval"],
              [ndb_binlog="default"])
  AC_ARG_WITH([ndb-jtie],
              [AC_HELP_STRING([--with-ndb-jtie],
                              [Include the NDB Cluster java-bindings for ClusterJ])],
              [ndb_jtie="$withval"],
              [ndb_jtie="no"])

  case "$ndb_ccflags" in
    "yes")
        AC_MSG_RESULT([The --ndb-ccflags option requires a parameter (passed to CC for ndb compilation)])
        ;;
    *)
        ndb_cxxflags_fix="$ndb_cxxflags_fix $ndb_ccflags"
    ;;
  esac

  AC_MSG_CHECKING([for NDB Cluster options])
  AC_MSG_RESULT([])
                                                                                
  have_ndb_test=no
  case "$ndb_test" in
    yes )
      AC_MSG_RESULT([-- including ndbapi test programs])
      have_ndb_test="yes"
      ;;
    * )
      AC_MSG_RESULT([-- not including ndbapi test programs])
      ;;
  esac

  have_ndb_docs=no
  case "$ndb_docs" in
    yes )
      AC_MSG_RESULT([-- including ndbapi and mgmapi documentation])
      have_ndb_docs="yes"
      ;;
    * )
      AC_MSG_RESULT([-- not including ndbapi and mgmapi documentation])
      ;;
  esac

  case "$ndb_debug" in
    yes )
      AC_MSG_RESULT([-- including ndb extra debug options])
      have_ndb_debug="yes"
      ;;
    full )
      AC_MSG_RESULT([-- including ndb extra extra debug options])
      have_ndb_debug="full"
      ;;
    no )
      AC_MSG_RESULT([-- not including ndb extra debug options])
      have_ndb_debug="no"
      ;;
    * )
      have_ndb_debug="default"
      ;;
  esac

  AC_MSG_CHECKING([for java needed for ndb-jtie])
  AC_MSG_RESULT([])
  MYSQL_CHECK_NDB_JTIE
  have_ndb_jtie=no
  case "$ndb_jtie" in
    yes )
      if test X"$ndb_jtie_supported" = Xyes
      then
        AC_MSG_RESULT([-- including ndb-jtie])
        have_ndb_jtie=yes
      else
        AC_MSG_ERROR([Unable to locate java needed for ndb-jtie])
      fi
      ;;
    default )
      if test X"$ndb_jtie_supported" = Xyes
      then
         AC_MSG_RESULT([-- including ndbjtie])
         have_ndb_jtie=yes
      else
         AC_MSG_RESULT([-- not including ndb-jtie])
         have_ndb_jtie=no
      fi
      ;;
    * )
      AC_MSG_RESULT([-- not including ndb-jtie])
      ;;
  esac
 
  AC_MSG_RESULT([done.])
])

AC_DEFUN([NDBCLUSTER_WORKAROUNDS], [

  #workaround for Sun Forte/x86 see BUG#4681
  case $SYSTEM_TYPE-$MACHINE_TYPE-$ac_cv_prog_gcc in
    *solaris*-i?86-no)
      CFLAGS="$CFLAGS -DBIG_TABLES"
      CXXFLAGS="$CXXFLAGS -DBIG_TABLES"
      ;;
    *)
      ;;
  esac

  # workaround for Sun Forte compile problem for ndb
  case $SYSTEM_TYPE-$ac_cv_prog_gcc in
    *solaris*-no)
      ndb_cxxflags_fix="$ndb_cxxflags_fix -instances=static"
      ;;
    *)
      ;;
  esac

  # ndb fail for whatever strange reason to link Sun Forte/x86
  # unless using incremental linker
  case $SYSTEM_TYPE-$MACHINE_TYPE-$ac_cv_prog_gcc-$have_ndbcluster in
    *solaris*-i?86-no-yes)
      CXXFLAGS="$CXXFLAGS -xildon"
      ;;
    *)
      ;;
  esac
])


AC_DEFUN([MYSQL_SETUP_NDBCLUSTER], [

  AC_MSG_RESULT([Using NDB Cluster])
  with_partition="yes"
  ndb_cxxflags_fix=""
  ndbcluster_includes="-I\$(top_builddir)/storage/ndb/include -I\$(top_srcdir)/storage/ndb/include -I\$(top_srcdir)/storage/ndb/include/ndbapi -I\$(top_srcdir)/storage/ndb/include/mgmapi"
  ndbcluster_libs="\$(top_builddir)/storage/ndb/src/.libs/libndbclient.a"
  ndbcluster_system_libs=""
  ndbcluster_sql_defines=""

  MYSQL_CHECK_NDB_OPTIONS
  NDB_CHECK_NDBMTD

  # checking CLOCK_MONOTONIC support
  AC_CHECK_FUNCS(clock_gettime pthread_condattr_setclock)

  # checking various functions
  AC_CHECK_FUNCS(pthread_self \
    sched_get_priority_min sched_get_priority_max sched_setaffinity \
    sched_setscheduler processor_bind epoll_create \
    posix_memalign memalign sysconf directio atomic_swap_32 mlock \
    ffs pthread_mutexattr_init pthread_mutexattr_settype)

  AC_MSG_CHECKING(for Linux scheduling and locking support)
  AC_TRY_LINK(
    [#ifndef _GNU_SOURCE
     #define _GNU_SOURCE
     #endif
     #include <sys/types.h>
     #include <unistd.h>
     #include <sched.h>
     #include <sys/syscall.h>],
    [const cpu_set_t *p= (const cpu_set_t*)0;
     struct sched_param loc_sched_param;
     int policy = 0;
     pid_t tid = (unsigned)syscall(SYS_gettid);
     tid = getpid();
     int ret = sched_setaffinity(tid, sizeof(* p), p);
     ret = sched_setscheduler(tid, policy, &loc_sched_param);],
    AC_MSG_RESULT(yes)
    AC_DEFINE(HAVE_LINUX_SCHEDULING, [1], [Linux scheduling/locking function]),
    AC_MSG_RESULT(no))

  AC_MSG_CHECKING(for Solaris affinity support)
  AC_TRY_LINK(
    [#include <sys/types.h>
     #include <sys/lwp.h>
     #include <sys/processor.h>
     #include <sys/procset.h>],
    [processorid_t cpu_id = (processorid_t)0;
     id_t tid = _lwp_self();
     int ret = processor_bind(P_LWPID, tid, cpu_id, 0);],
    AC_MSG_RESULT(yes)
    AC_DEFINE(HAVE_SOLARIS_AFFINITY, [1], [Solaris affinity function]),
    AC_MSG_RESULT(no))

  AC_MSG_CHECKING(for Linux futex support)
  AC_TRY_LINK(
    [#ifndef _GNU_SOURCE
     #define _GNU_SOURCE
     #endif
     #include <sys/types.h>
     #include <unistd.h>
     #include <errno.h>
     #include <sys/syscall.h>],
     #define FUTEX_WAIT        0
     #define FUTEX_WAKE        1
     #define FUTEX_FD          2
     #define FUTEX_REQUEUE     3
     #define FUTEX_CMP_REQUEUE 4
     #define FUTEX_WAKE_OP     5
    [
     int a = 0; int * addr = &a;
     return syscall(SYS_futex, addr, FUTEX_WAKE, 1, 0, 0, 0) == 0 ? 0 : errno;
    ],
    AC_MSG_RESULT(yes)
    AC_DEFINE(HAVE_LINUX_FUTEX, [1], [Linux futex support]),
    AC_MSG_RESULT(no))

  AC_CHECK_HEADERS(sun_prefetch.h)

  NDBCLUSTER_WORKAROUNDS
  NDB_COMPILER_FEATURES

  MAKE_BINARY_DISTRIBUTION_OPTIONS="$MAKE_BINARY_DISTRIBUTION_OPTIONS --with-ndbcluster"

  if test "$have_ndb_debug" = "default"
  then
    have_ndb_debug=$with_debug
  fi

  if test "$have_ndb_debug" = "yes"
  then
    # Medium debug.
    NDB_DEFS="-DNDB_DEBUG -DVM_TRACE -DERROR_INSERT -DARRAY_GUARD"
  elif test "$have_ndb_debug" = "full"
  then
    NDB_DEFS="-DNDB_DEBUG_FULL -DVM_TRACE -DERROR_INSERT -DARRAY_GUARD -DAPI_TRACE"
  else
    # no extra ndb debug but still do asserts if debug version
    if test "$with_debug" = "yes" -o "$with_debug" = "full"
    then
      NDB_DEFS=""
    else
      ndbcluster_sql_defines="-DNDEBUG"
      NDB_DEFS="-DNDEBUG"
    fi
  fi

  have_ndb_binlog="no"
  if test X"$ndb_binlog" = Xdefault ||
     test X"$ndb_binlog" = Xyes
  then
    have_ndb_binlog="yes"
  fi

  if test X"$have_ndb_binlog" = Xyes
  then
    AC_DEFINE([WITH_NDB_BINLOG], [1],
              [Including Ndb Cluster Binlog])
    AC_MSG_RESULT([Including Ndb Cluster Binlog])
  else
    AC_MSG_RESULT([Not including Ndb Cluster Binlog])
  fi

  ndb_transporter_opt_objs=""
  if test "$ac_cv_func_shmget" = "yes" &&
     test "$ac_cv_func_shmat" = "yes" &&
     test "$ac_cv_func_shmdt" = "yes" &&
     test "$ac_cv_func_shmctl" = "yes" &&
     test "$ac_cv_func_sigaction" = "yes" &&
     test "$ac_cv_func_sigemptyset" = "yes" &&
     test "$ac_cv_func_sigaddset" = "yes" &&
     test "$ac_cv_func_pthread_sigmask" = "yes"
  then
     AC_DEFINE([NDB_SHM_TRANSPORTER], [1],
               [Including Ndb Cluster DB shared memory transporter])
     AC_MSG_RESULT([Including ndb shared memory transporter])
     ndb_transporter_opt_objs="$ndb_transporter_opt_objs SHM_Transporter.lo SHM_Transporter.unix.lo"
  else
     AC_MSG_RESULT([Not including ndb shared memory transporter])
  fi
  
  if test X"$have_ndb_sci" = Xyes
  then
    ndb_transporter_opt_objs="$ndb_transporter_opt_objs SCI_Transporter.lo"
  fi

  ndb_opt_subdirs=
  ndb_bin_am_ldflags="-static"
  if test X"$have_ndb_test" = Xyes
  then
    ndb_opt_subdirs="test"
    ndb_bin_am_ldflags=""
  fi

  if test X"$have_ndb_docs" = Xyes
  then
    ndb_opt_subdirs="$ndb_opt_subdirs docs"
    ndb_bin_am_ldflags=""
  fi

  if test X"$have_ndb_jtie" = Xyes
  then
    ndb_opt_subdirs="$ndb_opt_subdirs ndbjtie"
  fi

  # building dynamic breaks on AIX. (If you want to try it and get unresolved
  # __vec__delete2 and some such, try linking against libhC.)
  case "$host_os" in
    aix3.* | aix4.0.* | aix4.1.*) ;;
    *) ndb_bin_am_ldflags="-static";;
  esac

  # libndbclient versioning when linked with GNU ld.
  if $LD --version 2>/dev/null|grep GNU >/dev/null 2>&1 ; then
    NDB_LD_VERSION_SCRIPT="-Wl,--version-script=\$(top_builddir)/storage/ndb/src/libndb.ver"
    AC_CONFIG_FILES(storage/ndb/src/libndb.ver)
  fi
  AC_SUBST(NDB_LD_VERSION_SCRIPT)

  AC_SUBST(NDB_SHARED_LIB_MAJOR_VERSION)
  AC_SUBST(NDB_SHARED_LIB_VERSION)

  # Replace @NDB_VERSION_XX@ variables in the generated ndb_version.h
  AC_SUBST(NDB_VERSION_MAJOR)
  AC_SUBST(NDB_VERSION_MINOR)
  AC_SUBST(NDB_VERSION_BUILD)
  AC_SUBST(NDB_VERSION_STATUS)

  # Define NDB_VERSION_XX variables in config.h/my_config.h
  AC_DEFINE_UNQUOTED([NDB_VERSION_MAJOR], [$NDB_VERSION_MAJOR],
                     [NDB major version])
  AC_DEFINE_UNQUOTED([NDB_VERSION_MINOR], [$NDB_VERSION_MINOR],
                     [NDB minor version])
  AC_DEFINE_UNQUOTED([NDB_VERSION_BUILD], [$NDB_VERSION_BUILD],
                     [NDB build version])
  AC_DEFINE_UNQUOTED([NDB_VERSION_STATUS], ["$NDB_VERSION_STATUS"],
                     [NDB status version])

  # Generate ndb_version.h from ndb_version.h.in
  AC_CONFIG_FILES([storage/ndb/include/ndb_version.h])

  AC_SUBST(ndbcluster_includes)
  AC_SUBST(ndbcluster_libs)
  AC_SUBST(ndbcluster_system_libs)
  AC_SUBST(NDB_SCI_LIBS)
  AC_SUBST(ndbcluster_sql_defines)

  AC_SUBST(ndb_transporter_opt_objs)
  AC_SUBST(ndb_bin_am_ldflags)
  AC_SUBST(ndb_opt_subdirs)

  AC_SUBST(NDB_DEFS)
  AC_SUBST(ndb_cxxflags_fix)

  NDB_SIZEOF_CHARP="$ac_cv_sizeof_charp"
  NDB_SIZEOF_CHAR="$ac_cv_sizeof_char"
  NDB_SIZEOF_SHORT="$ac_cv_sizeof_short"
  NDB_SIZEOF_INT="$ac_cv_sizeof_int"
  NDB_SIZEOF_LONG="$ac_cv_sizeof_long"
  NDB_SIZEOF_LONG_LONG="$ac_cv_sizeof_long_long"
  AC_SUBST([NDB_SIZEOF_CHARP])
  AC_SUBST([NDB_SIZEOF_CHAR])
  AC_SUBST([NDB_SIZEOF_SHORT])
  AC_SUBST([NDB_SIZEOF_INT])
  AC_SUBST([NDB_SIZEOF_LONG])
  AC_SUBST([NDB_SIZEOF_LONG_LONG])

  AC_CONFIG_FILES([storage/ndb/include/ndb_types.h])

])

