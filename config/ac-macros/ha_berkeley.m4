dnl ---------------------------------------------------------------------------
dnl Macro: MYSQL_CHECK_BDB
dnl Sets HAVE_BERKELEY_DB if inst library is found
dnl Makes sure db version is correct.
dnl Looks in $srcdir for Berkeley distribution if not told otherwise
dnl ---------------------------------------------------------------------------


AC_DEFUN([MYSQL_SETUP_BERKELEY_DB], [
  AC_ARG_WITH([berkeley-db],
              [
  --with-berkeley-db[=DIR]
                          Use BerkeleyDB located in DIR],
              [bdb="$withval"],
              [bdb=yes])

  AC_ARG_WITH([berkeley-db-includes],
              [
  --with-berkeley-db-includes=DIR
                          Find Berkeley DB headers in DIR],
              [bdb_includes="$withval"],
              [bdb_includes=default])

  AC_ARG_WITH([berkeley-db-libs],
              [
  --with-berkeley-db-libs=DIR
                          Find Berkeley DB libraries in DIR],
              [bdb_libs="$withval"],
              [bdb_libs=default])

	#  echo " bdb $bdb  $bdb_includes---$bdb_libs "
  case "$bdb" in
    yes )
      case "$bdb_includes---$bdb_libs" in
        default---default )
          mode=search-$bdb
          ;;
        default---* | *---default | yes---* | *---yes )
          AC_MSG_ERROR([if either 'includes' or 'libs' is specified, both must be specified])
          ;;
        * )
          mode=supplied-two
          ;;
      esac
      ;;
    * )
      mode=supplied-one
      ;;
  esac

  case $mode in
    supplied-two )
      MYSQL_CHECK_INSTALLED_BDB([$bdb_includes], [$bdb_libs])
      case $bdb_dir_ok in
        installed ) mode=yes ;;
        * ) AC_MSG_ERROR([didn't find valid BerkeleyDB: $bdb_dir_ok]) ;;
      esac
      ;;
    supplied-one )
      MYSQL_CHECK_BDB_DIR([$bdb])
      case $bdb_dir_ok in
        source ) mode=compile ;;
        installed ) mode=yes ;;
        * ) AC_MSG_ERROR([didn't find valid BerkeleyDB: $bdb_dir_ok]) ;;
      esac
      ;;
    search-* )
      MYSQL_SEARCH_FOR_BDB
      case $bdb_dir_ok in
        source ) mode=compile ;;
        installed ) mode=yes ;;
        * ) AC_MSG_ERROR([no suitable BerkeleyDB found]) ;;
      esac
      ;;
    *)
      AC_MSG_ERROR([impossible case condition '$mode': please report this to bugs@lists.mysql.com])
      ;;
  esac

  case $mode in
    yes )
      have_berkeley_db="yes"
      AC_MSG_RESULT([Using Berkeley DB in '$bdb_includes'])
      ;;
    compile )
      have_berkeley_db="$bdb"
      AC_MSG_RESULT([Compiling Berekeley DB in '$have_berkeley_db'])
      ;;
    * )
      AC_MSG_ERROR([impossible case condition '$mode': please report this to bugs@lists.mysql.com])
      ;;
  esac

      bdb_conf_flags="--disable-shared --build=$build_alias"
      if test $with_debug = "yes"
      then
        bdb_conf_flags="$bdb_conf_flags --enable-debug --enable-diagnostic"
      fi
      # NOTICE: if you're compiling BDB, it needs to be a SUBDIR
      # of $srcdir (i.e., you can 'cd $srcdir/$bdb').  It won't
      # work otherwise.
      if test -d "$bdb"; then :
      else
        # This should only happen when doing a VPATH build
        echo "NOTICE: I have to make the BDB directory: `pwd`:$bdb"
        mkdir "$bdb" || exit 1
      fi
      if test -d "$bdb"/build_unix; then :
      else
        # This should only happen when doing a VPATH build
        echo "NOTICE: I have to make the build_unix directory: `pwd`:$bdb/build_unix"
        mkdir "$bdb/build_unix" || exit 1
      fi
      rel_srcdir=
      case "$srcdir" in
        /* ) rel_srcdir="$srcdir" ;;
        * )  rel_srcdir="../../../$srcdir" ;;
      esac
      (cd $bdb/build_unix && \
       sh $rel_srcdir/$bdb/dist/configure $bdb_conf_flags) || \
        AC_MSG_ERROR([could not configure Berkeley DB])
 
  mysql_se_libs="$mysql_se_libs $bdb_libs_with_path" 

  AC_SUBST(bdb_includes)
  AC_SUBST(bdb_libs)
  AC_SUBST(bdb_libs_with_path)
  AC_CONFIG_FILES(storage/bdb/Makefile)
])

AC_DEFUN([MYSQL_CHECK_INSTALLED_BDB], [
dnl echo ["MYSQL_CHECK_INSTALLED_BDB ($1) ($2)"]
  inc="$1"
  lib="$2"
  if test -f "$inc/db.h"
  then
    MYSQL_CHECK_BDB_VERSION([$inc/db.h],
      [.*#define[ 	]*], [[ 	][ 	]*])

    if test X"$bdb_version_ok" = Xyes; then
      save_LDFLAGS="$LDFLAGS"
      LDFLAGS="-L$lib $LDFLAGS"
      AC_CHECK_LIB(db,db_env_create, [
        bdb_dir_ok=installed
        MYSQL_TOP_BUILDDIR([inc])
        MYSQL_TOP_BUILDDIR([lib])
        bdb_includes="-I$inc"
        bdb_libs="-L$lib -ldb"
        bdb_libs_with_path="$lib/libdb.a"
      ])
      LDFLAGS="$save_LDFLAGS"
    else
      bdb_dir_ok="$bdb_version_ok"
    fi
  else
    bdb_dir_ok="no db.h file in '$inc'"
  fi
])

AC_DEFUN([MYSQL_CHECK_BDB_DIR], [
dnl ([$bdb])
dnl echo ["MYSQL_CHECK_BDB_DIR ($1)"]
  dir="$1"

  MYSQL_CHECK_INSTALLED_BDB([$dir/include], [$dir/lib])

  if test X"$bdb_dir_ok" != Xinstalled; then
    # test to see if it's a source dir
    rel="$dir/dist/RELEASE"
    if test -f "$rel"; then
      MYSQL_CHECK_BDB_VERSION([$rel], [], [=])
      if test X"$bdb_version_ok" = Xyes; then
        bdb_dir_ok=source
        bdb="$dir"
        MYSQL_TOP_BUILDDIR([dir])
        bdb_includes="-I$dir/build_unix"
        bdb_libs="-L$dir/build_unix -ldb"
	bdb_libs_with_path="$dir/build_unix/libdb.a"
      else
        bdb_dir_ok="$bdb_version_ok"
      fi
    else
      bdb_dir_ok="'$dir' doesn't look like a BDB directory ($bdb_dir_ok)"
    fi
  fi
])

AC_DEFUN([MYSQL_SEARCH_FOR_BDB], [
dnl echo ["MYSQL_SEARCH_FOR_BDB"]
  bdb_dir_ok="no BerkeleyDB found"

  for test_dir in $srcdir/storage/bdb $srcdir/db-*.*.* /usr/local/BerkeleyDB*; do
dnl    echo "-----------> Looking at ($test_dir; `cd $test_dir && pwd`)"
    MYSQL_CHECK_BDB_DIR([$test_dir])
    if test X"$bdb_dir_ok" = Xsource || test X"$bdb_dir_ok" = Xinstalled; then
dnl	echo "-----------> Found it ($bdb), ($srcdir)"
dnl     This is needed so that 'make distcheck' works properly (VPATH build).
dnl     VPATH build won't work if bdb is not under the source tree; but in
dnl     that case, hopefully people will just make and install inside the
dnl     tree, or install BDB first, and then use the installed version.
	case "$bdb" in
	"$srcdir/"* ) bdb=`echo "$bdb" | sed -e "s,^$srcdir/,,"` ;;
	esac
        break
    fi
  done
])

dnl MYSQL_CHECK_BDB_VERSION takes 3 arguments:
dnl     1)  the file to look in
dnl     2)  the search pattern before DB_VERSION_XXX
dnl     3)  the search pattern between DB_VERSION_XXX and the number
dnl It assumes that the number is the last thing on the line
AC_DEFUN([MYSQL_CHECK_BDB_VERSION], [
  db_major=`sed -e '/^[$2]DB_VERSION_MAJOR[$3]/ !d' -e 's///' [$1]`
  db_minor=`sed -e '/^[$2]DB_VERSION_MINOR[$3]/ !d' -e 's///' [$1]`
  db_patch=`sed -e '/^[$2]DB_VERSION_PATCH[$3]/ !d' -e 's///' [$1]`
  test -z "$db_major" && db_major=0
  test -z "$db_minor" && db_minor=0
  test -z "$db_patch" && db_patch=0

dnl   # This is ugly, but about as good as it can get
dnl #  mysql_bdb=
dnl #  if test $db_major -eq 3 && test $db_minor -eq 2 && test $db_patch -eq 3
dnl #  then
dnl #    mysql_bdb=h
dnl #  elif test $db_major -eq 3 && test $db_minor -eq 2 && test $db_patch -eq 9
dnl #  then
dnl #    want_bdb_version="3.2.9a"	# hopefully this will stay up-to-date
dnl #    mysql_bdb=a
dnl #  fi

dnl RAM:
want_bdb_version="4.1.24"
bdb_version_ok=yes

dnl #  if test -n "$mysql_bdb" && \
dnl #	grep "DB_VERSION_STRING.*:.*$mysql_bdb: " [$1] > /dev/null
dnl #  then
dnl #    bdb_version_ok=yes
dnl #  else
dnl #    bdb_version_ok="invalid version $db_major.$db_minor.$db_patch"
dnl #    bdb_version_ok="$bdb_version_ok (must be version 3.2.3h or $want_bdb_version)"
dnl #  fi
])

AC_DEFUN([MYSQL_TOP_BUILDDIR], [
  case "$[$1]" in
    /* ) ;;		# don't do anything with an absolute path
    "$srcdir"/* )
      # If BDB is under the source directory, we need to look under the
      # build directory for bdb/build_unix.
      # NOTE: I'm being lazy, and assuming the user did not specify
      # something like --with-berkeley-db=bdb (it would be missing "./").
      [$1]="\$(top_builddir)/"`echo "$[$1]" | sed -e "s,^$srcdir/,,"`
      ;;
    * )
      AC_MSG_ERROR([The BDB directory must be directly under the MySQL source directory, or be specified using the full path. ('$srcdir'; '$[$1]')])
      ;;
  esac
  if test X"$[$1]" != "/"
  then
    [$1]=`echo $[$1] | sed -e 's,/$,,'`
  fi
])

dnl ---------------------------------------------------------------------------
dnl END OF MYSQL_CHECK_BDB SECTION
dnl ---------------------------------------------------------------------------
