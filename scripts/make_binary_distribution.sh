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

# There are platforms, notably OS X on Intel (x86 + x86_64),
# for which "uname" does not provide sufficient information.
# The value of CFLAGS as used during compilation is the most exact info
# we can get - after all, we care about _what_ we built, not _where_ we did it.
cflags="@CFLAGS@"

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
system=`echo $system | sed -e 's/darwin9.*/osx10.5/g'`
system=`echo $system | sed -e 's/\(aix4.3\).*/\1/g'`
system=`echo $system | sed -e 's/\(aix5.1\).*/\1/g'`
system=`echo $system | sed -e 's/\(aix5.2\).*/\1/g'`
system=`echo $system | sed -e 's/\(aix5.3\).*/\1/g'`
system=`echo $system | sed -e 's/osf5.1b/tru64/g'`
system=`echo $system | sed -e 's/linux-gnu/linux/g'`
system=`echo $system | sed -e 's/solaris2.\([0-9]*\)/solaris\1/g'`
system=`echo $system | sed -e 's/sco3.2v\(.*\)/openserver\1/g'`

# Get the "machine", which really is the CPU architecture (including the size).
# The precedence is:
# 1) use an explicit argument, if given;
# 2) use platform-specific fixes, if there are any (see bug#37808);
# 3) stay with the default (determined during "configure", using predefined macros).
if [ x"$MACHINE" != x"" ] ; then
  machine=$MACHINE
else
  case $system in
    osx* )
      # Extract "XYZ" from CFLAGS "... -arch XYZ ...", or empty!
      cflag_arch=`echo "$cflags" | sed -n -e 's=.* -arch \([^ ]*\) .*=\1=p'`
      case "$cflag_arch" in
        i386 )    case $system in
                    osx10.4 )  machine=i686 ;; # Used a different naming
                    * )        machine=x86 ;;
                  esac ;;
        x86_64 )  machine=x86_64 ;;
        ppc )     ;;  # No treatment needed with PPC
        ppc64 )   ;;
        * ) # No matching compiler flag? "--platform" is needed
            if [ x"$PLATFORM" != x"" ] ; then
              :  # See below: "$PLATFORM" will take precedence anyway
            elif [ "$system" = "osx10.3" -a -z "$cflag_arch" ] ; then
              :  # Special case of OS X 10.3, which is PPC-32 only and doesn't use "-arch"
            else
              echo "On system '$system' only specific '-arch' values are expected."
              echo "It is taken from the 'CFLAGS' whose value is:"
              echo "$cflags"
              echo "'-arch $cflag_arch' is unexpected, and no '--platform' was given: ABORT"
              exit 1
            fi ;;
      esac  # "$cflag_arch"
      ;;
  esac  # $system
fi

# Combine OS and CPU to the "platform". Again, an explicit argument takes precedence.
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
if [ "$?" = "1" -o x"$tar" = x"" ] ; then
  tar=tar
fi


mkdir $BASE $BASE/bin $BASE/docs \
 $BASE/include $BASE/lib $BASE/support-files $BASE/share $BASE/scripts \
 $BASE/mysql-test $BASE/mysql-test/t  $BASE/mysql-test/r \
 $BASE/mysql-test/include $BASE/mysql-test/std_data $BASE/mysql-test/lib \
 $BASE/mysql-test/suite

if [ $BASE_SYSTEM != "netware" ] ; then
 mkdir $BASE/share/mysql $BASE/tests $BASE/sql-bench $BASE/man \
  $BASE/man/man1 $BASE/man/man8 $BASE/data $BASE/data/mysql $BASE/data/test

 chmod o-rwx $BASE/data $BASE/data/*
fi

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
  myisam/myisamchk$BS myisam/myisampack$BS myisam/myisamlog$BS \
  myisam/myisam_ftdump$BS \
  sql/mysqld$BS sql/mysqld-debug$BS \
  sql/mysql_tzinfo_to_sql$BS \
  server-tools/instance-manager/mysqlmanager$BS \
  client/mysql$BS client/mysqlshow$BS client/mysqladmin$BS \
  client/mysqldump$BS client/mysqlimport$BS \
  client/mysqltest$BS client/mysqlcheck$BS \
  client/mysqlbinlog$BS client/mysql_upgrade$BS \
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
    server-tools/instance-manager/.libs/mysqlmanager \
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
if [ $BASE_SYSTEM = "netware" ] ; then
    for i in $BASE/lib/*.a
    do
      libname=`basename $i .a`
      $MV $i $BASE/lib/$libname.lib
    done
    rm -f $BASE/lib/*.la
fi

copyfileto $BASE/include include/*

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
    $CP man/*.8 $BASE/man/man8
    # In a Unix binary package, these tools and their manuals are not useful
    rm -f $BASE/man/man1/make_win_*
  fi
fi

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
$CP mysql-test/t/*.test mysql-test/t/*.imtest \
    mysql-test/t/*.disabled mysql-test/t/*.opt \
    mysql-test/t/*.slave-mi mysql-test/t/*.sh mysql-test/t/*.sql $BASE/mysql-test/t
$CP mysql-test/r/*.result mysql-test/r/*.require \
    $BASE/mysql-test/r

# Copy the additional suites and data "as is", they are in flux
$tar cf - mysql-test/suite    | ( cd $BASE ; $tar xf - )
$tar cf - mysql-test/std_data | ( cd $BASE ; $tar xf - )
# Clean up if we did this from a bk tree
if [ -d mysql-test/SCCS ] ; then
  find $BASE/mysql-test -name SCCS -print | xargs rm -rf
fi

if [ $BASE_SYSTEM != "netware" ] ; then
  chmod a+x $BASE/bin/*
  copyfileto $BASE/bin scripts/*
  $BASE/bin/replace \@localstatedir\@ ./data \@bindir\@ ./bin \@scriptdir\@ \
      ./bin \@libexecdir\@ ./bin \@sbindir\@ ./bin \@prefix\@ . \@HOSTNAME\@ \
      @HOSTNAME@ \@pkgdatadir\@ ./share \
      < scripts/mysql_install_db.sh > $BASE/scripts/mysql_install_db
  $BASE/bin/replace \@prefix\@ /usr/local/mysql \@bindir\@ ./bin \
      \@sbindir\@ ./bin \@libexecdir\@ ./bin \
      \@MYSQLD_USER\@ @MYSQLD_USER@ \@localstatedir\@ /usr/local/mysql/data \
      \@HOSTNAME\@ @HOSTNAME@ \
      < support-files/mysql.server.sh > $BASE/support-files/mysql.server
  $BASE/bin/replace /my/gnu/bin/hostname /bin/hostname -- $BASE/bin/mysqld_safe
  mv $BASE/support-files/binary-configure $BASE/configure
  chmod a+x $BASE/bin/* $BASE/scripts/* $BASE/support-files/mysql-log-rotate \
      $BASE/support-files/*.server $BASE/configure
  $CP -r sql-bench/* $BASE/sql-bench
  rm -f $BASE/sql-bench/*.sh $BASE/sql-bench/Makefile* $BASE/lib/*.la
  rm -f $BASE/bin/*.sql
fi

rm -f $BASE/bin/Makefile* $BASE/bin/*.in $BASE/bin/*.sh \
    $BASE/bin/mysql_install_db $BASE/bin/make_binary_distribution \
    $BASE/bin/make_win_* \
    $BASE/bin/setsomevars $BASE/support-files/Makefile* \
    $BASE/support-files/*.sh

#
# Copy system dependent files
#
if [ $BASE_SYSTEM = "netware" ] ; then
  ./scripts/fill_help_tables < ./Docs/manual.texi >> ./netware/init_db.sql
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
        $BASE/support-files/build-tags \
        $BASE/support-files/MySQL-shared-compat.spec \
        $BASE/support-files/ndb-config-2-node.ini \
        $BASE/INSTALL-BINARY
fi

# Make safe_mysqld a symlink to mysqld_safe for backwards portability
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
  ( cd ndb            ; @MAKE@ DESTDIR=$BASE/ndb-stage install pkglibdir=@pkglibdir@ )
  ( cd mysql-test/ndb ; @MAKE@ DESTDIR=$BASE/ndb-stage install pkglibdir=@pkglibdir@ )
  $CP $BASE/ndb-stage@bindir@/* $BASE/bin/.
  $CP $BASE/ndb-stage@libexecdir@/* $BASE/bin/.
  $CP $BASE/ndb-stage@pkglibdir@/* $BASE/lib/.
  $CP $BASE/ndb-stage@pkgdatadir@/* $BASE/share/mysql/.
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
  gcclib=`@CC@ @CFLAGS@ --print-libgcc-file 2>/dev/null` || true
  if [ -z "$gcclib" ] ; then
    echo "Warning: Compiler doesn't tell libgcc.a!"
  elif [ -f "$gcclib" ] ; then
    $CP $gcclib $BASE/lib/libmygcc.a
  else
    echo "Warning: Compiler result '$gcclib' not found / no file!"
  fi
fi

#if we are debugging, do not do tar/gz
if [ x$DEBUG = x1 ] ; then
 exit
fi

if [ $BASE_SYSTEM != "netware" ] ; then

  #
  # Create the result tar file
  #

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
