#!/bin/sh
# Copyright (C) 2000-2006 MySQL AB
# 
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; version 2 of the License.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

##############################################################################
#
#  This is a script to create a TAR or ZIP binary distribution out of a
#  built source tree. The output file will be put at the top level of
#  the source tree, as "mysql-<vsn>....{tar.gz,zip}"
#
#  Note that the structure created by this script is slightly different from
#  what a normal "make install" would produce. No extra "mysql" sub directory
#  will be created, i.e. no "$prefix/include/mysql", "$prefix/lib/mysql" or
#  "$prefix/share/mysql".  This is because the build system explicitly calls
#  make with pkgdatadir=<datadir>, etc.
#
#  In GNU make/automake terms
#
#    "pkglibdir"     is set to the same as "libdir"
#    "pkgincludedir" is set to the same as "includedir"
#    "pkgdatadir"    is set to the same as "datadir"
#    "pkgsuppdir"    is set to "@prefix@/support-files",
#                    normally the same as "datadir"
#
#  The temporary directory path given to "--tmp=<path>" has to be
#  absolute and with no spaces.
#
#  Note that for best result, the original "make" should be done with
#  the same arguments as used for "make install" below, especially the
#  'pkglibdir', as the RPATH should to be set correctly.
#
##############################################################################

##############################################################################
#
#  Read the command line arguments that control this script
#
##############################################################################

machine=@MACHINE_TYPE@
system=@SYSTEM_TYPE@
SOURCE=`pwd`
CP="cp -p"
MV="mv"

STRIP=1				# Option ignored
SILENT=0
PLATFORM=""
TMP=/tmp
SUFFIX=""
NDBCLUSTER=""			# Option ignored

for arg do
  case "$arg" in
    --tmp=*)    TMP=`echo "$arg" | sed -e "s;--tmp=;;"` ;;
    --suffix=*) SUFFIX=`echo "$arg" | sed -e "s;--suffix=;;"` ;;
    --no-strip) STRIP=0 ;;
    --machine=*) machine=`echo "$arg" | sed -e "s;--machine=;;"` ;;
    --platform=*) PLATFORM=`echo "$arg" | sed -e "s;--platform=;;"` ;;
    --silent)   SILENT=1 ;;
    --with-ndbcluster) NDBCLUSTER=1 ;;
    *)
      echo "Unknown argument '$arg'"
      exit 1
      ;;
  esac
done

# ----------------------------------------------------------------------
# Adjust "system" output from "uname" to be more human readable
# ----------------------------------------------------------------------

if [ x"$PLATFORM" = x"" ] ; then
  # FIXME move this to the build tools
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

  PLATFORM="$system-$machine"
fi

# Print the platform name for build logs
echo "PLATFORM NAME: $PLATFORM"

case $PLATFORM in
  *netware*) BASE_SYSTEM="netware" ;;
esac

# Change the distribution to a long descriptive name
NEW_NAME=mysql@MYSQL_SERVER_SUFFIX@-@VERSION@-$PLATFORM$SUFFIX

# ----------------------------------------------------------------------
# Define BASE, and remove the old BASE directory if any
# ----------------------------------------------------------------------
BASE=$TMP/my_dist$SUFFIX
if [ -d $BASE ] ; then
 rm -rf $BASE
fi

# ----------------------------------------------------------------------
# Find the TAR to use
# ----------------------------------------------------------------------

# This is needed to prefer GNU tar over platform tar because that can't
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

tar=`which_1 gnutar gtar`
if [ $? -ne 0 -o x"$tar" = x"" ] ; then
  tar=tar
fi


##############################################################################
#
#  Handle the Unix/Linux packaging using "make install"
#
##############################################################################

if [ x"$BASE_SYSTEM" != x"netware" ] ; then

  # ----------------------------------------------------------------------
  # Terminate on any base level error
  # ----------------------------------------------------------------------
  set -e

  # ----------------------------------------------------------------------
  # Really ugly, one script, "mysql_install_db", needs prefix set to ".",
  # i.e. makes access relative the current directory. This matches
  # the documentation, so better not change this. And for another script,
  # "mysql.server", we make some relative, others not.
  # ----------------------------------------------------------------------

  cd scripts
  rm -f mysql_install_db
  @MAKE@ mysql_install_db \
    prefix=. \
    bindir=./bin \
    sbindir=./bin \
    scriptdir=./bin \
    libexecdir=./bin \
    pkgdatadir=./share \
    localstatedir=./data
  cd ..

  cd support-files
  rm -f mysql.server
  @MAKE@ mysql.server \
    bindir=./bin \
    sbindir=./bin \
    scriptdir=./bin \
    libexecdir=./bin \
    pkgdatadir=@pkgdatadir@
  cd ..

  # ----------------------------------------------------------------------
  # Do a install that we later are to pack. Use the same paths as in
  # the build for the relevant directories.
  # ----------------------------------------------------------------------
  @MAKE@ DESTDIR=$BASE install \
    pkglibdir=@pkglibdir@ \
    pkgincludedir=@pkgincludedir@ \
    pkgdatadir=@pkgdatadir@ \
    pkgsuppdir=@pkgsuppdir@ \
    mandir=@mandir@ \
    infodir=@infodir@

  # ----------------------------------------------------------------------
  # Rename top directory, and set DEST to the new directory
  # ----------------------------------------------------------------------
  mv $BASE@prefix@ $BASE/$NEW_NAME
  DEST=$BASE/$NEW_NAME

  # ----------------------------------------------------------------------
  # If we compiled with gcc, copy libgcc.a to the dist as libmygcc.a
  # ----------------------------------------------------------------------
  if [ x"@GXX@" = x"yes" ] ; then
    gcclib=`@CC@ @CFLAGS@ --print-libgcc-file`
    if [ $? -ne 0 ] ; then
      echo "Warning: Couldn't find libgcc.a!"
    else
      $CP $gcclib $DEST/lib/libmygcc.a
    fi
  fi

  # FIXME let this script be in "bin/", where it is in the RPMs?
  # http://dev.mysql.com/doc/refman/5.1/en/mysql-install-db-problems.html
  mkdir $DEST/scripts
  mv $DEST/bin/mysql_install_db $DEST/scripts/

  # Note, no legacy "safe_mysqld" link to "mysqld_safe" in 5.1

  # Copy readme and license files
  cp README Docs/INSTALL-BINARY  $DEST/
  if [ -f COPYING -a -f EXCEPTIONS-CLIENT ] ; then
    cp COPYING EXCEPTIONS-CLIENT $DEST/
  elif [ -f LICENSE.mysql ] ; then
    cp LICENSE.mysql $DEST/
  else
    echo "ERROR: no license files found"
    exit 1
  fi

  # FIXME should be handled by make file, and to other dir
  mkdir -p $DEST/bin $DEST/support-files
  cp scripts/mysqlaccess.conf $DEST/bin/
  cp support-files/magic      $DEST/support-files/

  # Create empty data directories, set permission (FIXME why?)
  mkdir       $DEST/data $DEST/data/mysql $DEST/data/test
  chmod o-rwx $DEST/data $DEST/data/mysql $DEST/data/test

  # ----------------------------------------------------------------------
  # Create the result tar file
  # ----------------------------------------------------------------------

  echo "Using $tar to create archive"
  OPT=cvf
  if [ x$SILENT = x1 ] ; then
    OPT=cf
  fi

  echo "Creating and compressing archive"
  rm -f $NEW_NAME.tar.gz
  (cd $BASE ; $tar $OPT -  $NEW_NAME) | gzip -9 > $NEW_NAME.tar.gz
  echo "$NEW_NAME.tar.gz created"

  echo "Removing temporary directory"
  rm -rf $BASE
  exit 0
fi


##############################################################################
#
#  Handle the Netware case, until integrated above
#
##############################################################################

BS=".nlm"
MYSQL_SHARE=$BASE/share

mkdir $BASE $BASE/bin $BASE/docs \
 $BASE/include $BASE/lib $BASE/support-files $BASE/share $BASE/scripts \
 $BASE/mysql-test $BASE/mysql-test/t  $BASE/mysql-test/r \
 $BASE/mysql-test/include $BASE/mysql-test/std_data $BASE/mysql-test/lib \
 $BASE/mysql-test/suite

# Copy files if they exists, warn for those that don't.
# Note that when listing files to copy, we might list the file name
# twice, once in the directory location where it is built, and a
# second time in the ".libs" location. In the case the first one
# is a wrapper script, the second one will overwrite it with the
# binary file.
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
         EXCEPTIONS-CLIENT LICENSE.mysql

# Non platform-specific bin dir files:
BIN_FILES="extra/comp_err$BS extra/replace$BS extra/perror$BS \
  extra/resolveip$BS extra/my_print_defaults$BS \
  extra/resolve_stack_dump$BS extra/mysql_waitpid$BS \
  storage/myisam/myisamchk$BS storage/myisam/myisampack$BS \
  storage/myisam/myisamlog$BS storage/myisam/myisam_ftdump$BS \
  sql/mysqld$BS sql/mysqld-debug$BS \
  sql/mysql_tzinfo_to_sql$BS \
  server-tools/instance-manager/mysqlmanager$BS \
  client/mysql$BS client/mysqlshow$BS client/mysqladmin$BS \
  client/mysqlslap$BS \
  client/mysqldump$BS client/mysqlimport$BS \
  client/mysqltest$BS client/mysqlcheck$BS \
  client/mysqlbinlog$BS client/mysql_upgrade$BS \
  tests/mysql_client_test$BS \
  libmysqld/examples/mysql_client_test_embedded$BS \
  libmysqld/examples/mysqltest_embedded$BS \
  ";

# Platform-specific bin dir files:
BIN_FILES="$BIN_FILES \
    netware/mysqld_safe$BS netware/mysql_install_db$BS \
    netware/init_db.sql netware/test_db.sql \
    netware/mysqlhotcopy$BS netware/libmysql$BS netware/init_secure_db.sql \
    ";

copyfileto $BASE/bin $BIN_FILES

$CP netware/*.pl $BASE/scripts
$CP scripts/mysqlhotcopy $BASE/scripts/mysqlhotcopy.pl

copyfileto $BASE/lib \
  libmysql/.libs/libmysqlclient.a \
  libmysql/.libs/libmysqlclient.so* \
  libmysql/.libs/libmysqlclient.sl* \
  libmysql/.libs/libmysqlclient*.dylib \
  libmysql/libmysqlclient.* \
  libmysql_r/.libs/libmysqlclient_r.a \
  libmysql_r/.libs/libmysqlclient_r.so* \
  libmysql_r/.libs/libmysqlclient_r.sl* \
  libmysql_r/.libs/libmysqlclient_r*.dylib \
  libmysql_r/libmysqlclient_r.* \
  libmysqld/.libs/libmysqld.a \
  libmysqld/.libs/libmysqld.so* \
  libmysqld/.libs/libmysqld.sl* \
  libmysqld/.libs/libmysqld*.dylib \
  mysys/libmysys.a strings/libmystrings.a dbug/libdbug.a \
  libmysqld/libmysqld.a netware/libmysql.imp \
  zlib/.libs/libz.a

# convert the .a to .lib for NetWare
for i in $BASE/lib/*.a
do
  libname=`basename $i .a`
  $MV $i $BASE/lib/$libname.lib
done
rm -f $BASE/lib/*.la


copyfileto $BASE/include config.h include/*

rm -f $BASE/include/Makefile* $BASE/include/*.in $BASE/include/config-win.h

copyfileto $BASE/support-files support-files/*

copyfileto $BASE/share scripts/*.sql

$CP -r sql/share/* $MYSQL_SHARE
rm -f $MYSQL_SHARE/Makefile* $MYSQL_SHARE/*/*.OLD

copyfileto $BASE/mysql-test \
         mysql-test/mysql-test-run mysql-test/install_test_db \
         mysql-test/mysql-test-run.pl mysql-test/README \
         mysql-test/mysql-stress-test.pl \
         mysql-test/valgrind.supp \
         netware/mysql_test_run.nlm netware/install_test_db.ncf

$CP mysql-test/lib/*.pl  $BASE/mysql-test/lib
$CP mysql-test/t/*.def $BASE/mysql-test/t
$CP mysql-test/include/*.inc $BASE/mysql-test/include
$CP mysql-test/include/*.test $BASE/mysql-test/include
$CP mysql-test/t/*.def $BASE/mysql-test/t
$CP mysql-test/std_data/*.dat mysql-test/std_data/*.frm \
    mysql-test/std_data/*.MYD mysql-test/std_data/*.MYI \
    mysql-test/std_data/*.pem mysql-test/std_data/Moscow_leap \
    mysql-test/std_data/Index.xml \
    mysql-test/std_data/des_key_file mysql-test/std_data/*.*001 \
    mysql-test/std_data/*.cnf mysql-test/std_data/*.MY* \
    $BASE/mysql-test/std_data
$CP mysql-test/t/*.test mysql-test/t/*.imtest \
    mysql-test/t/*.disabled mysql-test/t/*.opt \
    mysql-test/t/*.slave-mi mysql-test/t/*.sh mysql-test/t/*.sql $BASE/mysql-test/t
$CP mysql-test/r/*.result mysql-test/r/*.require \
    $BASE/mysql-test/r

# Copy the additional suites "as is", they are in flux
$tar cf - mysql-test/suite | ( cd $BASE ; $tar xf - )
# Clean up if we did this from a bk tree
if [ -d mysql-test/SCCS ] ; then
  find $BASE/mysql-test -name SCCS -print | xargs rm -rf
fi

rm -f $BASE/bin/Makefile* $BASE/bin/*.in $BASE/bin/*.sh \
    $BASE/bin/mysql_install_db $BASE/bin/make_binary_distribution \
    $BASE/bin/setsomevars $BASE/support-files/Makefile* \
    $BASE/support-files/*.sh

#
# Copy system dependent files
#
./scripts/fill_help_tables < ./Docs/manual.texi >> ./netware/init_db.sql

#
# Remove system dependent files
#
rm -f   $BASE/support-files/magic \
        $BASE/support-files/mysql.server \
        $BASE/support-files/mysql*.spec \
        $BASE/support-files/mysql-log-rotate \
        $BASE/support-files/binary-configure \
        $BASE/support-files/build-tags \
	$BASE/support-files/MySQL-shared-compat.spec \
        $BASE/INSTALL-BINARY

# Clean up if we did this from a bk tree
if [ -d $BASE/sql-bench/SCCS ] ; then
  find $BASE/share -name SCCS -print | xargs rm -rf
  find $BASE/sql-bench -name SCCS -print | xargs rm -rf
fi

BASE2=$TMP/$NEW_NAME
rm -rf $BASE2
mv $BASE $BASE2
BASE=$BASE2

#
# Create a zip file for NetWare users
#
rm -f $NEW_NAME.zip
(cd $TMP; zip -r "$SOURCE/$NEW_NAME.zip" $NEW_NAME)
echo "$NEW_NAME.zip created"

echo "Removing temporary directory"
rm -rf $BASE
