# The NDB version number and status.
# Should be updated when creating a new NDB version
NDB_VERSION_MAJOR=7
NDB_VERSION_MINOR=1
NDB_VERSION_BUILD=27
NDB_VERSION_STATUS=""

dnl ---------------------------------------------------------------------------
dnl Macro: MYSQL_CHECK_CXX_LINKING
dnl ---------------------------------------------------------------------------
AC_DEFUN([MYSQL_CHECK_CXX_LINKING], [
  # Check if linking need additional C++ libraries since
  # we are (most likely) linking with gcc
  ndb_cxx_runtime_libs=""

  AC_MSG_CHECKING(how to link C++ programs)

  LIBS_save="$LIBS"
  AC_LANG_PUSH(C++)

  for L in "" -lstdc++
  do
    LIBS="$LIBS $L"
    AC_LINK_IFELSE(
      AC_LANG_PROGRAM([],
      [char* p=new char;delete p;]),
      [ linked_ok=yes ], [])
    LIBS="$LIBS_save"

    if test X$linked_ok = Xyes
    then
      ndb_cxx_runtime_libs="$L"
      ndb_can_link_cxx_program=yes
      break;
    fi
  done

  AC_LANG_POP(C++)

  AC_MSG_RESULT([$ndb_cxx_runtime_libs])
  AC_SUBST(ndb_cxx_runtime_libs)
])

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


AC_DEFUN([MYSQL_CHECK_JAVA], [

  NDB_JAVA_PATHS="$JAVA_HOME $JDK_HOME"
  NDB_JAVA_TMP_INC="include"
  NDB_JAVA_TMP_BIN="bin"

  case "$host_os" in
  darwin*)        
    AC_CHECK_FILE([/usr/libexec/java_home],[found=yes])
    if test X$found = Xyes
    then
      # MaxOS >= 1.5
      NDB_JAVA_PATHS="$NDB_JAVA_PATHS `/usr/libexec/java_home`"
    else
      NDB_JAVA_TMP_INC="Headers"
      NDB_JAVA_TMP_BIN="Commands"
      NDB_JAVA_PATHS="$NDB_JAVA_PATHS /System/Library/Frameworks/JavaVM.framework/Versions/CurrentJDK"
    fi
    ;;
  *)
    NDB_JAVA_PATHS="$NDB_JAVA_PATHS /usr/lib/jvm/java /usr/lib64/jvm/java"
    NDB_JAVA_PATHS="$NDB_JAVA_PATHS /usr/local/jdk /usr/local/java /usr/local/java/jdk"
    NDB_JAVA_PATHS="$NDB_JAVA_PATHS /usr/jdk/latest"
    ;;
  esac

  dnl
  dnl Search for JAVA_HOME
  dnl

  NDB_JAVA_INC=""
  NDB_JAVA_BIN=""

  for D in $NDB_JAVA_PATHS; do
    AC_CHECK_FILE([$D/$NDB_JAVA_TMP_INC/jni.h],[found=yes])
    if test X$found = Xyes
    then
      NDB_JAVA_INC=$D/$NDB_JAVA_TMP_INC
      NDB_JAVA_BIN=$D/$NDB_JAVA_TMP_BIN
      break;
    fi
  done

  echo "$NDB_JAVA_INC"

  if test -f "${NDB_JAVA_INC}/jni.h"
  then
    JNI_INCLUDE_DIRS="-I${NDB_JAVA_INC}"
  else
    AC_MSG_RESULT([-- Unable to locate jni.h!])
  fi

  dnl try to add extra include path
  case "$host_os" in
  bsdi*)    JNI_SUBDIRS="bsdos";;
  freebsd*) JNI_SUBDIRS="freebsd";;
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
    if test -d "${NDB_JAVA_INC}/${S}"
    then
      JNI_INCLUDE_DIRS="${JNI_INCLUDE_DIRS} -I${NDB_JAVA_INC}/${S}"
    fi
  done

  CPPFLAGS_save="$CPPFLAGS"
  CPPFLAGS="$CPPFLAGS ${JNI_INCLUDE_DIRS}"
  AC_CHECK_HEADERS(jni.h)
  CPPFLAGS="$CPPFLAGS_save"

  ndb_java_supported=no

  if test X$NDB_JAVA_BIN != X
  then

    AC_PATH_PROG(JAVAC, javac, no, ${NDB_JAVA_BIN})
    AC_PATH_PROG(JAVAH, javah, no, ${NDB_JAVA_BIN})
    AC_PATH_PROG(JAR, jar, no, ${NDB_JAVA_BIN})
    AC_PATH_PROG(JAVA, java, no, ${NDB_JAVA_BIN})
    AC_SUBST(JNI_INCLUDE_DIRS)

    # generated java unit tests need to know whether to run 32 or 64 bit
    JAVA_ARCH=`expr $ac_cv_sizeof_charp \* 8`
    AC_SUBST([JAVA_ARCH])
    Java_JAVA_ARCH_OPT=-d${JAVA_ARCH}
    AC_SUBST([Java_JAVA_ARCH_OPT])
    Java_JAVA_EXECUTABLE_PATH=${JAVA}
    AC_SUBST([Java_JAVA_EXECUTABLE_PATH])

    if test X"$JAVAC" != Xno &&
      test X"$JAVAH" != Xno && 
      test X"$JAR" != Xno && 
      test X"$JAVA" != Xno && 
      test X"$ac_cv_header_jni_h" = Xyes
    then
       ndb_java_supported=yes
    fi
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
  case "$ndb_ccflags" in
    "yes")
        AC_MSG_RESULT([The --ndb-ccflags option requires a parameter (passed to CC for ndb compilation)])
        ;;
    *)
        ndb_cxxflags_fix="$ndb_cxxflags_fix $ndb_ccflags"
    ;;
  esac

  AC_ARG_WITH([ndb-binlog],
              [AC_HELP_STRING([--without-ndb-binlog],
                              [Disable ndb binlog])],
              [ndb_binlog="$withval"],
              [ndb_binlog="default"])
  AC_ARG_WITH([openjpa],
              [AS_HELP_STRING([--with-openjpa],
	                      [Include and set path for native
                               OpenJPA support])],
              [openjpa="$withval"],
              [openjpa="default"])
  AC_ARG_WITH([classpath],
              [AS_HELP_STRING([--with-classpath=PATH],
                              [Include and set classpath for Cluster/J,
                               Cluster/J JPA, and Cluster/J JDBC])],
              [classpath="$withval"],
              [classpath="no"])
  AC_ARG_WITH([javac-target],
              [AC_HELP_STRING([--with-javac-target],
                              [Java compiler target version to be used])],
              [javac_target="$withval"],
              [javac_target="1.5"])

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

  MYSQL_CHECK_CXX_LINKING

  AC_MSG_CHECKING([for Java needed for ClusterJ and ClusterJPA])
  AC_MSG_RESULT([])
  MYSQL_CHECK_JAVA
  NDBJTIE_LIBS=""


  have_clusterj=no
  if test X"$ndb_java_supported" = Xyes
  then
    if echo $CHARSETS | grep ucs2 >/dev/null
    then
      AC_MSG_RESULT([-- including Cluster/J])
      have_clusterj=yes
    else
      AC_MSG_WARN([-- Cluster/J requires ucs2 charset;
                   use --with-extra-charsets to configure])
      have_clusterj=yes
    fi
  else
    AC_MSG_RESULT([-- Cluster/J requires Java and JNI: Cluster/J not included])
  fi

  have_classpath=no
  if test X"$classpath" != Xyes && test X"$classpath" != Xno && test X"$classpath" != Xdefault
  then
    AC_MSG_RESULT([-- including provided classpath: $classpath])
    have_classpath=$classpath;
  elif test X"$CLASSPATH" != X
  then
    AC_MSG_RESULT([-- including CLASSPATH from environment: $CLASSPATH])
    have_classpath="$CLASSPATH"
    classpath="$CLASSPATH"
  fi

  # needed for junit test compile: 
  #   junit-4.7.jaropenjpa-1.2.1.jar
 
  # needed for OpenJPA compile:
  #   openjpa-x.y.z.jar:geronimo-jpa_x.y_spec-x.y.jar

  # needed for ClusterJ JDBC compile:
  #   antlr-3.2.jar
  #   antlr-runtime-3.2.jar
  #   antlr-2.7.7.jar
  #   stringtemplate-3.2.jar

  # needed for PCEnhancement:
  #   serp-x.y.z.jar:commons-lang-x.y.jar:geronimo-jta_x.y_spec-x.y.jar:commons-collections-x.y.jar
  
  have_junit=no
  have_openjpa_jar=no
  TMP_CLASSPATH=`echo $classpath | sed 's/:/ /'`;
  for i in $TMP_CLASSPATH; do
    if `echo $i | egrep "junit-(.+)\.jar" 1>/dev/null 2>&1`
    then
      AC_MSG_RESULT([-- junit found: activating clusterj tests])
      have_junit=yes
    fi
    if `echo $i | egrep "openjpa-(.+)\.jar" 1>/dev/null 2>&1`
    then
      AC_MSG_RESULT([-- openjpa jar found: activating clusterjpa])
      have_openjpa_jar=yes
    fi
    if `echo $i | egrep "(.+)-jpa-(.+)\.jar" 1>/dev/null 2>&1`
    then
      AC_MSG_RESULT([-- jpa jar found: activating clusterjpa])
      have_jpa_jar=yes
    fi
  done

  have_openjpa=no
  if test X"$openjpa" != Xno
  then
    if test X"$have_clusterj" = Xyes
    then
      if test X"$have_openjpa_jar" != Xno
      then
        # no test of actual classpath validity for now
        AC_MSG_RESULT([-- including OpenJPA])
        have_openjpa=yes
      else
        AC_MSG_RESULT([-- Cluster for OpenJPA requires external
                       OpenJPA jar set with --with-classpath: not included])
      fi
    else
      AC_MSG_RESULT([-- Cluster for OpenJPA requires Cluster/J and
                     Java to compile: not included])
    fi
  fi

  if test x"$have_clusterj" = xyes
  then
    NDBJTIE_OPT="ndbjtie"
    NDBJTIE_LIBS="ndbjtie/libndbjtie.la ndbjtie/mysql/libmysqlutils.la"

    # generate jtie unit tests:
    AC_CONFIG_FILES([storage/ndb/src/ndbjtie/jtie/test/myapi/test_myapi.sh],
           [chmod +x storage/ndb/src/ndbjtie/jtie/test/myapi/test_myapi.sh])
    AC_CONFIG_FILES([storage/ndb/src/ndbjtie/jtie/test/myjapi/test_myjapi.sh],
           [chmod +x storage/ndb/src/ndbjtie/jtie/test/myjapi/test_myjapi.sh])
    AC_CONFIG_FILES([storage/ndb/src/ndbjtie/jtie/test/unload/test_unload.sh],
           [chmod +x storage/ndb/src/ndbjtie/jtie/test/unload/test_unload.sh])

    # generate ndbjtie unit tests:
    AC_CONFIG_FILES([storage/ndb/src/ndbjtie/test/test_mutils.sh],
           [chmod +x storage/ndb/src/ndbjtie/test/test_mutils.sh])
    AC_CONFIG_FILES([storage/ndb/src/ndbjtie/test/test_ndbjtie_constants.sh],
           [chmod +x storage/ndb/src/ndbjtie/test/test_ndbjtie_constants.sh])
    AC_CONFIG_FILES([storage/ndb/src/ndbjtie/test/test_ndbjtie_multilib.sh],
           [chmod +x storage/ndb/src/ndbjtie/test/test_ndbjtie_multilib.sh])
    AC_CONFIG_FILES([storage/ndb/src/ndbjtie/test/test_ndbjtie_smoke.sh],
           [chmod +x storage/ndb/src/ndbjtie/test/test_ndbjtie_smoke.sh])
    AC_CONFIG_FILES([storage/ndb/src/ndbjtie/test/test_unload_mutils.sh],
           [chmod +x storage/ndb/src/ndbjtie/test/test_unload_mutils.sh])
    AC_CONFIG_FILES([storage/ndb/src/ndbjtie/test/test_unload_ndbjtie_constants.sh],
           [chmod +x storage/ndb/src/ndbjtie/test/test_unload_ndbjtie_constants.sh])
    AC_CONFIG_FILES([storage/ndb/src/ndbjtie/test/test_unload_ndbjtie_multilib.sh],
           [chmod +x storage/ndb/src/ndbjtie/test/test_unload_ndbjtie_multilib.sh])
    AC_CONFIG_FILES([storage/ndb/src/ndbjtie/test/test_unload_ndbjtie_smoke.sh],
           [chmod +x storage/ndb/src/ndbjtie/test/test_unload_ndbjtie_smoke.sh])
  fi

  if test x"$have_openjpa" = xyes  
  then
    OPENJPA_OPT="clusterj-openjpa"
  fi

  if test x"$have_junit" == xyes 
  then
    CLUSTERJ_TESTS="clusterj-test"
  fi

  if test X"$have_openjpa" != Xno && test X"$have_junit" = Xyes
  then
    CLUSTERJ_TESTS="$CLUSTERJ_TESTS clusterj-jpatest"
  fi

  # switch to enable experimental support for ClusterJ-JDBC
  AC_ARG_WITH([clusterj-jdbc],
              [AS_HELP_STRING([--with-clusterj-jdbc],
                              [Include experimental support for
                               ClusterJ JDBC])],
              [clusterj_jdbc="$withval"],
              [clusterj_jdbc="no"])

  have_clusterj_jdbc=no
  if test X"$clusterj_jdbc" != Xno
  then
    if test X"$have_clusterj" = Xyes
    then
      AC_MSG_RESULT([-- including ClusterJ JDBC])
      have_clusterj_jdbc=yes
    else
      AC_MSG_RESULT([-- ClusterJ for JDBC requires ClusterJ: not included])
    fi
  else
    AC_MSG_RESULT([-- ClusterJ for JDBC is only included
                   when --with-clusterj-jdbc is specified: not included])
  fi

  if test x"$have_clusterj_jdbc" = xyes  
  then
    CLUSTERJ_JDBC_OPT="clusterj-jdbc"
  fi

  AC_SUBST(CLUSTERJ_JDBC_OPT)


  AC_SUBST(NDBJTIE_OPT)
  AC_SUBST(NDBJTIE_LIBS)
  AC_SUBST(CLUSTERJ_TESTS)
  AC_SUBST(OPENJPA_OPT)

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

  if test X"$have_clusterj" = Xyes
  then
    ndb_opt_subdirs="$ndb_opt_subdirs clusterj"
  fi
  if test X"$have_openjpa" != Xno
  then
    CLUSTERJ_OPENJPA=$have_openjpa
    AC_SUBST(CLUSTERJ_OPENJPA)
  fi
  if test X"$have_classpath" != Xno
  then
    CLUSTERJ_CLASSPATH=$have_classpath
    AC_SUBST(CLUSTERJ_CLASSPATH)
  fi
  JAVAC_TARGET=$javac_target
  AC_SUBST(JAVAC_TARGET)
  

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

  # Build the version string used for creating jars etc.
  JAVA_NDB_VERSION=$NDB_VERSION_MAJOR.$NDB_VERSION_MINOR.$NDB_VERSION_BUILD
  if test X"$NDB_VERSION_STATUS" != X
  then
    JAVA_NDB_VERSION=$JAVA_NDB_VERSION.$NDB_VERSION_STATUS
  fi
  AC_SUBST(JAVA_NDB_VERSION)

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

