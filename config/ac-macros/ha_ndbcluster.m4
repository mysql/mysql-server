dnl ---------------------------------------------------------------------------
dnl Macro: MYSQL_CHECK_NDBCLUSTER
dnl Sets HAVE_NDBCLUSTER_DB if --with-ndbcluster is used
dnl ---------------------------------------------------------------------------
                                                                                
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
        NDB_SCI_LIBS="-L$mysql_sci_dir/lib -lsisci"
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
              [
  --with-ndb-test       Include the NDB Cluster ndbapi test programs],
              [ndb_test="$withval"],
              [ndb_test=no])
  AC_ARG_WITH([ndb-docs],
              [
  --with-ndb-docs       Include the NDB Cluster ndbapi and mgmapi documentation],
              [ndb_docs="$withval"],
              [ndb_docs=no])
  AC_ARG_WITH([ndb-port],
              [
  --with-ndb-port       Port for NDB Cluster management server],
              [ndb_port="$withval"],
              [ndb_port="default"])
  AC_ARG_WITH([ndb-port-base],
              [
  --with-ndb-port-base  Base port for NDB Cluster transporters],
              [ndb_port_base="$withval"],
              [ndb_port_base="default"])
  AC_ARG_WITH([ndb-debug],
              [
  --without-ndb-debug   Disable special ndb debug features],
              [ndb_debug="$withval"],
              [ndb_debug="default"])
  AC_ARG_WITH([ndb-ccflags],
              AC_HELP_STRING([--with-ndb-ccflags=CFLAGS],
                           [Extra CFLAGS for ndb compile]),
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

  AC_MSG_RESULT([done.])
])

AC_DEFUN([MYSQL_CHECK_NDBCLUSTER], [
  AC_ARG_WITH([ndbcluster],
              [
  --with-ndbcluster        Include the NDB Cluster table handler],
              [ndbcluster="$withval"],
              [ndbcluster=no])
                                                                                
  AC_MSG_CHECKING([for NDB Cluster])
                                                                                
  have_ndbcluster=no
  ndbcluster_includes=
  ndbcluster_libs=
  ndb_mgmclient_libs=
  case "$ndbcluster" in
    yes )
      AC_MSG_RESULT([Using NDB Cluster])
      AC_DEFINE([HAVE_NDBCLUSTER_DB], [1], [Using Ndb Cluster DB])
      have_ndbcluster="yes"
      ndbcluster_includes="-I../ndb/include -I../ndb/include/ndbapi -I../ndb/include/mgmapi"
      ndbcluster_libs="\$(top_builddir)/ndb/src/.libs/libndbclient.a"
      ndbcluster_system_libs=""
      ndb_mgmclient_libs="\$(top_builddir)/ndb/src/mgmclient/libndbmgmclient.la"
      MYSQL_CHECK_NDB_OPTIONS
      ;;
    * )
      AC_MSG_RESULT([Not using NDB Cluster])
      ;;
  esac

  AM_CONDITIONAL([HAVE_NDBCLUSTER_DB], [ test "$have_ndbcluster" = "yes" ])
  AC_SUBST(ndbcluster_includes)
  AC_SUBST(ndbcluster_libs)
  AC_SUBST(ndbcluster_system_libs)
  AC_SUBST(ndb_mgmclient_libs)
])
                                                                                
dnl ---------------------------------------------------------------------------
dnl END OF MYSQL_CHECK_NDBCLUSTER SECTION
dnl ---------------------------------------------------------------------------
