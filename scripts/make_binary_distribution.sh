#!/bin/sh

# This is a script to create a TAR or ZIP binary distribution out of a
# built source tree. The output file will be put at the top level of
# the source tree, as "mysql-<vsn>....{tar.gz,zip}"
#
# The temporary directory path given to "--tmp=<path>" has to be
# absolute and with no spaces.

machine=@MACHINE_TYPE@
system=@SYSTEM_TYPE@
version=@VERSION@
SOURCE=`pwd`
CP="cp -p"
MV="mv"

STRIP=1
DEBUG=0
SILENT=0
MACHINE=""
PLATFORM=""
TMP=/tmp
SUFFIX=""
NDBCLUSTER=""

for arg do
  case "$arg" in
    --debug)    DEBUG=1;;
    --tmp=*)    TMP=`echo "$arg" | sed -e "s;--tmp=;;"` ;;
    --suffix=*) SUFFIX=`echo "$arg" | sed -e "s;--suffix=;;"` ;;
    --no-strip) STRIP=0 ;;
    --machine=*) MACHINE=`echo "$arg" | sed -e "s;--machine=;;"` ;;
    --platform=*) PLATFORM=`echo "$arg" | sed -e "s;--platform=;;"` ;;
    --silent)   SILENT=1 ;;
    --with-ndbcluster) NDBCLUSTER=1 ;;
    *)
      echo "Unknown argument '$arg'"
      exit 1
      ;;
  esac
done

# Remove vendor from $system
system=`echo $system | sed -e 's/[a-z]*-\(.*\)/\1/g'`

# Map OS names to "our" OS names (eg. darwin6.8 -> osx10.2)
system=`echo $system | sed -e 's/darwin6.*/osx10.2/g'`
system=`echo $system | sed -e 's/darwin7.*/osx10.3/g'`
system=`echo $system | sed -e 's/darwin8.*/osx10.4/g'`
system=`echo $system | sed -e 's/\(aix4.3\).*/\1/g'`
system=`echo $system | sed -e 's/\(aix5.1\).*/\1/g'`
system=`echo $system | sed -e 's/\(aix5.2\).*/\1/g'`
system=`echo $system | sed -e 's/\(aix5.3\).*/\1/g'`
system=`echo $system | sed -e 's/osf5.1b/tru64/g'`
system=`echo $system | sed -e 's/linux-gnu/linux/g'`
system=`echo $system | sed -e 's/solaris2.\([0-9]*\)/solaris\1/g'`
system=`echo $system | sed -e 's/sco3.2v\(.*\)/openserver\1/g'`

if [ x"$MACHINE" != x"" ] ; then
  machine=$MACHINE
fi

if [ x"$PLATFORM" != x"" ] ; then
  platform="$PLATFORM"
else
  platform="$system-$machine"
fi

# FIXME This should really be integrated with automake and not duplicate the
# installation list.

BASE=$TMP/my_dist$SUFFIX

if [ -d $BASE ] ; then
 rm -rf $BASE
fi

BS=""
BIN_FILES=""
BASE_SYSTEM="any"
MYSQL_SHARE=$BASE/share/mysql

case $system in
  *netware*)
    BASE_SYSTEM="netware"
    BS=".nlm"
    MYSQL_SHARE=$BASE/share
    ;;
esac


mkdir $BASE $BASE/bin $BASE/docs \
 $BASE/include $BASE/lib $BASE/support-files $BASE/share $BASE/scripts \
 $BASE/mysql-test $BASE/mysql-test/t  $BASE/mysql-test/r \
 $BASE/mysql-test/include $BASE/mysql-test/std_data $BASE/mysql-test/lib

if [ $BASE_SYSTEM != "netware" ] ; then
 mkdir $BASE/share/mysql $BASE/tests $BASE/sql-bench $BASE/man \
  $BASE/man/man1 $BASE/data $BASE/data/mysql $BASE/data/test

 chmod o-rwx $BASE/data $BASE/data/*
fi

# Copy files if they exists, warn for those that don't
copyfileto()
{
  destdir=$1
  shift
  for i
  do
    if [ -f $i ] ; then
      $CP $i $destdir
    elif [ -d $i ] ; then
      echo "Warning: Will not copy directory \"$i\""
    else
      echo "Warning: Listed file not found   \"$i\""
    fi
  done
}

copyfileto $BASE/docs ChangeLog Docs/mysql.info

copyfileto $BASE COPYING COPYING.LIB README Docs/INSTALL-BINARY \
         EXCEPTIONS-CLIENT MySQLEULA.txt LICENSE.doc README.NW

# Non platform-specific bin dir files:
BIN_FILES="extra/comp_err$BS extra/replace$BS extra/perror$BS \
  extra/resolveip$BS extra/my_print_defaults$BS \
  extra/resolve_stack_dump$BS extra/mysql_waitpid$BS \
  myisam/myisamchk$BS myisam/myisampack$BS myisam/myisamlog$BS \
  myisam/myisam_ftdump$BS \
  sql/mysqld$BS sql/mysql_tzinfo_to_sql$BS \
  server-tools/instance-manager/mysqlmanager$BS \
  client/mysql$BS client/mysqlshow$BS client/mysqladmin$BS \
  client/mysqldump$BS client/mysqlimport$BS \
  client/mysqltest$BS client/mysqlcheck$BS \
  client/mysqlbinlog$BS \
  tests/mysql_client_test$BS \
  libmysqld/examples/mysql_client_test_embedded$BS \
  libmysqld/examples/mysqltest_embedded$BS \
  ";

# Platform-specific bin dir files:
if [ $BASE_SYSTEM = "netware" ] ; then
  BIN_FILES="$BIN_FILES \
    netware/mysqld_safe$BS netware/mysql_install_db$BS \
    netware/init_db.sql netware/test_db.sql netware/mysql_explain_log$BS \
    netware/mysqlhotcopy$BS netware/libmysql$BS netware/init_secure_db.sql \
    ";
# For all other platforms:
else
  BIN_FILES="$BIN_FILES \
    client/mysqltestmanagerc \
    client/mysqltestmanager-pwgen tools/mysqltestmanager \
    client/.libs/mysql client/.libs/mysqlshow client/.libs/mysqladmin \
    client/.libs/mysqldump client/.libs/mysqlimport \
    client/.libs/mysqltest client/.libs/mysqlcheck \
    client/.libs/mysqlbinlog client/.libs/mysqltestmanagerc \
    client/.libs/mysqltestmanager-pwgen tools/.libs/mysqltestmanager \
    tests/.libs/mysql_client_test \
    libmysqld/examples/.libs/mysql_client_test_embedded \
    libmysqld/examples/.libs/mysqltest_embedded \
  ";
fi

copyfileto $BASE/bin $BIN_FILES

if [ x$STRIP = x1 ] ; then
  strip $BASE/bin/*
fi

# Copy not binary files
copyfileto $BASE/bin sql/mysqld.sym.gz

if [ $BASE_SYSTEM = "netware" ] ; then
    $CP netware/*.pl $BASE/scripts
    $CP scripts/mysqlhotcopy $BASE/scripts/mysqlhotcopy.pl
fi

copyfileto $BASE/lib \
  libmysql/.libs/libmysqlclient.a libmysql/.libs/libmysqlclient.so* \
  libmysql/libmysqlclient.* libmysql_r/.libs/libmysqlclient_r.a \
  libmysql_r/.libs/libmysqlclient_r.so* libmysql_r/libmysqlclient_r.* \
  mysys/libmysys.a strings/libmystrings.a dbug/libdbug.a \
  libmysqld/.libs/libmysqld.a libmysqld/.libs/libmysqld.so* \
  libmysqld/libmysqld.a netware/libmysql.imp \
  zlib/.libs/libz.a

# convert the .a to .lib for NetWare
if [ $BASE_SYSTEM = "netware" ] ; then
    for i in $BASE/lib/*.a
    do
      libname=`basename $i .a`
      $MV $i $BASE/lib/$libname.lib
    done
fi

copyfileto $BASE/include config.h include/*

rm -f $BASE/include/Makefile* $BASE/include/*.in $BASE/include/config-win.h
if [ $BASE_SYSTEM != "netware" ] ; then
  rm -f $BASE/include/config-netware.h
fi

if [ $BASE_SYSTEM != "netware" ] ; then
  if [ -d tests ] ; then
    $CP tests/*.res tests/*.tst tests/*.pl $BASE/tests
  fi
  if [ -d man ] ; then
    $CP man/*.1 $BASE/man/man1
  fi
fi

copyfileto $BASE/support-files support-files/*

copyfileto $BASE/share scripts/*.sql

$CP -r sql/share/* $MYSQL_SHARE
rm -f $MYSQL_SHARE/Makefile* $MYSQL_SHARE/*/*.OLD

copyfileto $BASE/mysql-test \
         mysql-test/mysql-test-run mysql-test/install_test_db \
         mysql-test/mysql-test-run.pl mysql-test/README \
	 mysql-test/valgrind.supp \
         netware/mysql_test_run.nlm netware/install_test_db.ncf

$CP mysql-test/lib/*.pl  $BASE/mysql-test/lib
$CP mysql-test/lib/*.sql $BASE/mysql-test/lib
$CP mysql-test/t/*.def $BASE/mysql-test/t
$CP mysql-test/include/*.inc $BASE/mysql-test/include
$CP mysql-test/t/*.def $BASE/mysql-test/t
$CP mysql-test/std_data/*.dat mysql-test/std_data/*.frm \
    mysql-test/std_data/*.pem mysql-test/std_data/Moscow_leap \
    mysql-test/std_data/des_key_file mysql-test/std_data/*.*001 \
    $BASE/mysql-test/std_data
$CP mysql-test/t/*.test mysql-test/t/*.disabled mysql-test/t/*.opt \
    mysql-test/t/*.slave-mi mysql-test/t/*.sh mysql-test/t/*.sql $BASE/mysql-test/t
$CP mysql-test/r/*.result mysql-test/r/*.require \
    $BASE/mysql-test/r

if [ $BASE_SYSTEM != "netware" ] ; then
  chmod a+x $BASE/bin/*
  copyfileto $BASE/bin scripts/*
  $BASE/bin/replace \@localstatedir\@ ./data \@bindir\@ ./bin \@scriptdir\@ \
      ./bin \@libexecdir\@ ./bin \@sbindir\@ ./bin \@prefix\@ . \@HOSTNAME\@ \
      @HOSTNAME@ \@pkgdatadir\@ ./support-files \
      < scripts/mysql_install_db.sh > $BASE/scripts/mysql_install_db
  $BASE/bin/replace \@prefix\@ /usr/local/mysql \@bindir\@ ./bin \
      \@sbindir\@ ./bin \@libexecdir\@ ./bin \
      \@MYSQLD_USER\@ @MYSQLD_USER@ \@localstatedir\@ /usr/local/mysql/data \
      \@HOSTNAME\@ @HOSTNAME@ \
      < support-files/mysql.server.sh > $BASE/support-files/mysql.server
  $BASE/bin/replace /my/gnu/bin/hostname /bin/hostname -- $BASE/bin/mysqld_safe
  mv $BASE/support-files/binary-configure $BASE/configure
  chmod a+x $BASE/bin/* $BASE/scripts/* $BASE/support-files/mysql-* \
      $BASE/support-files/mysql.server $BASE/configure
  $CP -r sql-bench/* $BASE/sql-bench
  rm -f $BASE/sql-bench/*.sh $BASE/sql-bench/Makefile* $BASE/lib/*.la
  rm -f $BASE/bin/*.sql
fi

rm -f $BASE/bin/Makefile* $BASE/bin/*.in $BASE/bin/*.sh \
    $BASE/bin/mysql_install_db $BASE/bin/make_binary_distribution \
    $BASE/bin/setsomevars $BASE/support-files/Makefile* \
    $BASE/support-files/*.sh

#
# Copy system dependent files
#
if [ $BASE_SYSTEM = "netware" ] ; then
  echo "CREATE DATABASE mysql;" > $BASE/bin/init_db.sql
  echo "CREATE DATABASE test;" >> $BASE/bin/init_db.sql
  sh ./scripts/mysql_create_system_tables.sh real >> $BASE/bin/init_db.sql
  sh ./scripts/mysql_create_system_tables.sh test > $BASE/bin/test_db.sql
fi

#
# Remove system dependent files
#
if [ $BASE_SYSTEM = "netware" ] ; then
  rm -f $BASE/support-files/magic \
        $BASE/support-files/mysql.server \
        $BASE/support-files/mysql*.spec \
        $BASE/support-files/mysql-log-rotate \
        $BASE/support-files/binary-configure \
        $BASE/INSTALL-BINARY \
        $BASE/MySQLEULA.txt
else
    rm -f $BASE/README.NW
fi

# Make safe_mysqld a symlink to mysqld_safe for backwards portability
# To be removed in MySQL 4.1
if [ $BASE_SYSTEM != "netware" ] ; then
  (cd $BASE/bin ; ln -s mysqld_safe safe_mysqld )
fi

# Clean up if we did this from a bk tree
if [ -d $BASE/sql-bench/SCCS ] ; then
  find $BASE/share -name SCCS -print | xargs rm -rf
  find $BASE/sql-bench -name SCCS -print | xargs rm -rf
fi

# NDB Cluster
if [ x$NDBCLUSTER = x1 ]; then
  ( cd ndb            ; @MAKE@ DESTDIR=$BASE/ndb-stage install )
  ( cd mysql-test/ndb ; @MAKE@ DESTDIR=$BASE/ndb-stage install )
  $CP $BASE/ndb-stage@bindir@/* $BASE/bin/.
  $CP $BASE/ndb-stage@libexecdir@/* $BASE/bin/.
  $CP $BASE/ndb-stage@pkglibdir@/* $BASE/lib/.
  $CP -r $BASE/ndb-stage@pkgincludedir@/ndb $BASE/include
  $CP -r $BASE/ndb-stage@prefix@/mysql-test/ndb $BASE/mysql-test/. || exit 1
  rm -rf $BASE/ndb-stage
fi

# Change the distribution to a long descriptive name
NEW_NAME=mysql@MYSQL_SERVER_SUFFIX@-$version-$platform$SUFFIX

# Print the platform name for build logs
echo "PLATFORM NAME: $platform"

BASE2=$TMP/$NEW_NAME
rm -rf $BASE2
mv $BASE $BASE2
BASE=$BASE2
#
# If we are compiling with gcc, copy libgcc.a to the distribution as libmygcc.a
#

if [ x"@GXX@" = x"yes" ] ; then
  gcclib=`@CC@ --print-libgcc-file`
  if [ $? -ne 0 ] ; then
    print "Warning: Couldn't find libgcc.a!"
  else
    $CP $gcclib $BASE/lib/libmygcc.a
  fi
fi

#if we are debugging, do not do tar/gz
if [ x$DEBUG = x1 ] ; then
 exit
fi

# This is needed to prefere gnu tar instead of tar because tar can't
# always handle long filenames

PATH_DIRS=`echo $PATH | \
    sed -e 's/^:/. /' -e 's/:$/ ./' -e 's/::/ . /g' -e 's/:/ /g' `

which_1 ()
{
  for cmd
  do
    for d in $PATH_DIRS
    do
      for file in $d/$cmd
      do
	if [ -x $file -a ! -d $file ] ; then
	  echo $file
	  exit 0
	fi
      done
    done
  done
  exit 1
}

if [ $BASE_SYSTEM != "netware" ] ; then

  #
  # Create the result tar file
  #

  tar=`which_1 gnutar gtar`
  if [ "$?" = "1" -o x"$tar" = x"" ] ; then
    tar=tar
  fi

  echo "Using $tar to create archive"

  OPT=cvf
  if [ x$SILENT = x1 ] ; then
    OPT=cf
  fi

  echo "Creating and compressing archive"
  rm -f $NEW_NAME.tar.gz
  (cd $TMP ; $tar $OPT -  $NEW_NAME) | gzip -9 > $NEW_NAME.tar.gz
  echo "$NEW_NAME.tar.gz created"

else

  #
  # Create a zip file for NetWare users
  #

  rm -f $NEW_NAME.zip
  (cd $TMP; zip -r "$SOURCE/$NEW_NAME.zip" $NEW_NAME)
  echo "$NEW_NAME.zip created"

fi

echo "Removing temporary directory"
rm -rf $BASE
