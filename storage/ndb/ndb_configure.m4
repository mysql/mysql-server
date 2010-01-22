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


dnl ---------------------------------------------------------------------------
dnl Macro: MYSQL_CHECK_NDBCLUSTER
dnl ---------------------------------------------------------------------------

NDB_MYSQL_VERSION_MAJOR=`echo $VERSION | cut -d. -f1`
NDB_MYSQL_VERSION_MINOR=`echo $VERSION | cut -d. -f2`
NDB_MYSQL_VERSION_BUILD=`echo $VERSION | cut -d. -f3 | cut -d- -f1`

TEST_NDBCLUSTER=""

dnl for build ndb docs

build_ndbmtd=
AC_PATH_PROG(DOXYGEN, doxygen, no)
AC_PATH_PROG(PDFLATEX, pdflatex, no)
AC_PATH_PROG(MAKEINDEX, makeindex, no)

AC_SUBST(DOXYGEN)
AC_SUBST(PDFLATEX)
AC_SUBST(MAKEINDEX)

AC_DEFUN([MYSQL_CHECK_NDB_JTIE], [

  case "$host_os" in
  darwin*)        
    NDB_JAVA_TMP_INC="Headers"
    NDB_JAVA_TMP_BIN="Commands"
    ;;
  *)
    NDB_JAVA_TMP_INC="include"
    NDB_JAVA_TMP_BIN="bin"
    ;;
  esac

  dnl
  dnl Search for JAVA_HOME
  dnl

  NDB_JAVA_INC=""
  NDB_JAVA_BIN=""
  for D in $JAVA_HOME $JDK_HOME /usr/lib/jvm/java /usr/lib64/jvm/java /usr/local/jdk /usr/local/java /usr/local/java/jdk /usr/jdk/latest /System/Library/Frameworks/JavaVM.framework/Versions/CurrentJDK ; do
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

  AC_PATH_PROG(JAVAC, javac, no, ${NDB_JAVA_BIN})
  AC_PATH_PROG(JAVAH, javah, no, ${NDB_JAVA_BIN})
  AC_PATH_PROG(JAR, jar, no, ${NDB_JAVA_BIN})
  AC_SUBST(JNI_INCLUDE_DIRS)

  ndb_java_supported=no
  if test X"$JAVAC" != Xno &&
     test X"$JAVAH" != Xno && 
     test X"$JAR" != Xno && 
     test X"$ac_cv_header_jni_h" = Xyes
  then
     ndb_java_supported=yes
  fi
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
  AC_ARG_WITH([ndb-jtie],
              [AC_HELP_STRING([--with-ndb-jtie],
                              [Include the NDB Cluster java-bindings for ClusterJ])],
              [ndb_jtie="$withval"],
              [ndb_jtie="default"]) 

  AC_ARG_WITH([ndbmtd],
              [AC_HELP_STRING([--without-ndbmtd],
                              [Dont build ndbmtd])],
              [ndb_mtd="$withval"],
              [ndb_mtd=yes])
  AC_ARG_WITH([clusterj], 
              [AC_HELP_STRING([--with-clusterj],
                              [Include Java API for Cluster/J])],
              [clusterj="$withval"],
              [clusterj="default"]) 
  AC_ARG_WITH([openjpa],
              [AS_HELP_STRING([--with-openjpa], Include and set path for native OpenJPA support)],
              [openjpa="$withval"],
              [openjpa="no"])
  AC_ARG_WITH([classpath],
              [AS_HELP_STRING([--with-classpath=PATH], Include and set classpath for Cluster/J and OpenJPA support)],
              [classpath="$withval"],
              [classpath="no"])

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

  AC_MSG_CHECKING([for java needed for ndb-jtie])
  AC_MSG_RESULT([])
  MYSQL_CHECK_NDB_JTIE
  have_ndb_jtie=no
  case "$ndb_jtie" in
    yes )
      if test X"$ndb_java_supported" = Xyes
      then
        AC_MSG_RESULT([-- including ndb-jtie])
        have_ndb_jtie=yes
      else
        AC_MSG_ERROR([Unable to locate java compiler needed for ndb-jtie])
      fi
      ;;
    default )
      if test X"$ndb_java_supported" = Xyes
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

  have_clusterj=no
  if test X"$clusterj" = Xyes || test X"$clusterj" = Xdefault
  then
    if test X"$ndb_java_supported" = Xyes
    then
      if test X"$have_ndb_jtie" = Xyes
      then
        AC_MSG_RESULT([-- including Cluster/J])
        have_clusterj=yes
      else
        AC_MSG_RESULT([-- Cluster/J requires ndb-jtie: Cluster/J not included])
      fi
    else
      AC_MSG_RESULT([-- Cluster/J requires a Java compiler: Cluster/J not included])
    fi
  else
    AC_MSG_RESULT([-- not including Cluster/J])
  fi

  have_classpath=no
  if test X"$classpath" != Xyes && test X"$classpath" != Xno && test X"$classpath" != Xdefault
  then
    AC_MSG_RESULT([-- including provided classpath])
    have_classpath=$classpath;
  fi
  
  have_openjpa=no
  if test X"$openjpa" = Xyes || test X"$openjpa" = Xdefault
  then
    if test X"$have_clusterj" = Xyes
    then
      if test X"$have_classpath" != Xno
      then
        # no test of actual classpath validity for now
        AC_MSG_RESULT([-- including OpenJPA])
        have_openjpa=yes
      else
        AC_MSG_RESULT([-- Cluster for OpenJPA requires external OpenJPA jar set with --with-classpath: not included])
      fi
    else
      AC_MSG_RESULT([-- Cluster for OpenJPA requires Cluster/J and a Java compiler: not included])
    fi
  fi

  AM_CONDITIONAL([COND_OPENJPA_OPT], [test x"$have_openjpa" != xno])
  AM_CONDITIONAL([COND_CLUSTERJ_TESTS], [test x"$have_classpath" != xno])
  AM_CONDITIONAL([COND_OPENJPA_TESTS], [test X"$have_openjpa" != Xno && test x"$have_classpath" != xno])

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
  TEST_NDBCLUSTER="--ndbcluster"

  ndbcluster_includes="-I\$(top_builddir)/storage/ndb/include -I\$(top_srcdir)/storage/ndb/include -I\$(top_srcdir)/storage/ndb/include/ndbapi -I\$(top_srcdir)/storage/ndb/include/mgmapi"
  ndbcluster_libs="\$(top_builddir)/storage/ndb/src/.libs/libndbclient.a"
  ndbcluster_system_libs=""
  ndb_mgmclient_libs="\$(top_builddir)/storage/ndb/src/mgmclient/libndbmgmclient.la"

  MYSQL_CHECK_NDB_OPTIONS
  NDBCLUSTER_WORKAROUNDS

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
  
  if test X"$ndb_mtd" = Xyes
  then
    if test X"$have_ndbmtd_asm" = Xyes
    then
      build_ndbmtd=yes
      AC_MSG_RESULT([Including ndbmtd])
    fi
  fi
  export build_ndbmtd

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

  AC_SUBST(NDB_VERSION_MAJOR)
  AC_SUBST(NDB_VERSION_MINOR)
  AC_SUBST(NDB_VERSION_BUILD)
  AC_SUBST(NDB_VERSION_STATUS)
  AC_DEFINE_UNQUOTED([NDB_VERSION_MAJOR], [$NDB_VERSION_MAJOR],
                     [NDB major version])
  AC_DEFINE_UNQUOTED([NDB_VERSION_MINOR], [$NDB_VERSION_MINOR],
                     [NDB minor version])
  AC_DEFINE_UNQUOTED([NDB_VERSION_BUILD], [$NDB_VERSION_BUILD],
                     [NDB build version])
  AC_DEFINE_UNQUOTED([NDB_VERSION_STATUS], ["$NDB_VERSION_STATUS"],
                     [NDB status version])

  JAVA_NDB_VERSION=$NDB_VERSION_MAJOR.$NDB_VERSION_MINOR.$NDB_VERSION_BUILD
  if test X"$JAVA_NDB_VERSION" != X
  then
    JAVA_NDB_VERSION=$JAVA_NDB_VERSION.$NDB_VERSION_STATUS
  fi

  AC_SUBST(JAVA_NDB_VERSION)
  AC_DEFINE_UNQUOTED([JAVA_NDB_VERSION], ["$JAVA_NDB_VERSION"],
                     [JAVA NDB version])

  AC_SUBST(NDB_MYSQL_VERSION_MAJOR)
  AC_SUBST(NDB_MYSQL_VERSION_MINOR)
  AC_SUBST(NDB_MYSQL_VERSION_BUILD)
  AC_DEFINE_UNQUOTED([NDB_MYSQL_VERSION_MAJOR], [$NDB_MYSQL_VERSION_MAJOR],
                     [MySQL major version])
  AC_DEFINE_UNQUOTED([NDB_MYSQL_VERSION_MINOR], [$NDB_MYSQL_VERSION_MINOR],
                     [MySQL minor version])
  AC_DEFINE_UNQUOTED([NDB_MYSQL_VERSION_BUILD], [$NDB_MYSQL_VERSION_BUILD],
                     [MySQL build version])

  AC_SUBST(ndbcluster_includes)
  AC_SUBST(ndbcluster_libs)
  AC_SUBST(ndbcluster_system_libs)
  AC_SUBST(ndb_mgmclient_libs)
  AC_SUBST(NDB_SCI_LIBS)

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

  AC_CONFIG_FILES([
   storage/ndb/include/ndb_version.h
   storage/ndb/include/ndb_types.h
  ])
])

AC_SUBST(TEST_NDBCLUSTER)

dnl ---------------------------------------------------------------------------
dnl END OF MYSQL_CHECK_NDBCLUSTER SECTION
dnl ---------------------------------------------------------------------------

