#!/bin/sh
# The default path should be /usr/local

# Get some info from configure
# chmod +x ./scripts/setsomevars

machine=@MACHINE_TYPE@
system=@SYSTEM_TYPE@
version=@VERSION@
export machine system version
SOURCE=`pwd` 

# Save temporary distribution here (must be full path) 
TMP=/tmp
if test $# -gt 0 -a x$1 != x"-debug" 
then
  TMP=$1
  shift 1
fi

if test x$1 = x"-debug"
then
  DEBUG=1
  shift 1
fi  


#make

# This should really be integrated with automake and not duplicate the
# installation list.

BASE=$TMP/my_dist

if [ -d $BASE ] ; then
 rm -rf $BASE
fi

mkdir $BASE $BASE/bin $BASE/data $BASE/data/mysql $BASE/data/test \
 $BASE/include $BASE/lib $BASE/support-files $BASE/share $BASE/share/mysql \
 $BASE/tests $BASE/scripts $BASE/sql-bench $BASE/mysql-test \
 $BASE/mysql-test/t  $BASE/mysql-test/r \
 $BASE/mysql-test/include $BASE/mysql-test/std_data
 
chmod o-rwx $BASE/data $BASE/data/*

for i in sql/ChangeLog COPYING COPYING.LIB README Docs/INSTALL-BINARY \
         Docs/manual.html Docs/manual.txt Docs/manual_toc.html
do
  cp -p $i $BASE
done

for i in extra/comp_err extra/replace extra/perror extra/resolveip \
 extra/my_print_defaults isam/isamchk isam/pack_isam myisam/myisamchk myisam/myisampack sql/mysqld sql/mysqlbinlog \
 client/mysql sql/mysqld client/mysqlshow client/mysqladmin client/mysqldump \
 client/mysqlimport client/mysqltest \
 client/.libs/mysql client/.libs/mysqlshow client/.libs/mysqladmin client/.libs/mysqldump client/.libs/mysqlimport client/.libs/mysqltest
do
  cp -p $i $BASE/bin
done

cp -p config.h include/* $BASE/include
rm $BASE/include/Makefile*; rm $BASE/include/*.in

cp -p tests/*.res tests/*.tst tests/*.pl $BASE/tests
cp -p support-files/* $BASE/support-files
cp -p libmysql/.libs/libmysqlclient.a libmysql/.libs/libmysqlclient.so* libmysql/libmysqlclient.* libmysql_r/.libs/libmysqlclient_r.a libmysql_r/.libs/libmysqlclient_r.so* libmysql_r/libmysqlclient_r.* mysys/libmysys.a strings/libmystrings.a dbug/libdbug.a $BASE/lib
cp -r -p sql/share/* $BASE/share/mysql
rm -f $BASE/share/mysql/Makefile* $BASE/share/mysql/*/*.OLD
rm -rf $BASE/share/SCCS  $BASE/share/*/SCCS 

cp -p mysql-test/mysql-test-run mysql-test/install_test_db $BASE/mysql-test/
cp -p mysql-test/README $BASE/mysql-test/README
cp -p mysql-test/include/*.inc $BASE/mysql-test/include
cp -p mysql-test/std_data/*.dat  mysql-test/std_data/*.frm \
      mysql-test/std_data/*.MRG  $BASE/mysql-test/std_data
cp -p mysql-test/t/*.test mysql-test/t/*.opt $BASE/mysql-test/t
cp -p mysql-test/r/*.result  $BASE/mysql-test/r

cp -p scripts/* $BASE/bin
rm -f $BASE/bin/Makefile* $BASE/bin/*.in $BASE/bin/*.sh $BASE/bin/mysql_install_db $BASE/bin/make_binary_distribution $BASE/bin/setsomevars $BASE/support-files/Makefile* $BASE/support-files/*.sh

$BASE/bin/replace \@localstatedir\@ ./data \@bindir\@ ./bin \@scriptdir\@ ./bin \@libexecdir\@ ./bin \@sbindir\@ ./bin \@prefix\@ . \@HOSTNAME\@ @HOSTNAME@ < $SOURCE/scripts/mysql_install_db.sh > $BASE/scripts/mysql_install_db
$BASE/bin/replace \@prefix\@ /usr/local/mysql \@bindir\@ ./bin \@MYSQLD_USER\@ root \@localstatedir\@ /usr/local/mysql/data < $SOURCE/support-files/mysql.server.sh > $BASE/support-files/mysql.server
$BASE/bin/replace /my/gnu/bin/hostname /bin/hostname -- $BASE/bin/safe_mysqld

mv $BASE/support-files/binary-configure $BASE/configure
chmod a+x $BASE/bin/* $BASE/scripts/* $BASE/support-files/mysql-* $BASE/configure
cp -r -p sql-bench/* $BASE/sql-bench
rm -f $BASE/sql-bench/*.sh $BASE/sql-bench/Makefile* $BASE/lib/*.la
rm -rf `find $BASE/sql-bench -name SCCS`
rm -rf `find $BASE/share -name SCCS`

# Change the distribution to a long descreptive name
NEW_NAME=mysql-$version-$system-$machine
BASE2=$TMP/$NEW_NAME
rm -rf $BASE2
mv $BASE $BASE2
BASE=$BASE2
#
# If we are compiling with gcc, copy libgcc.a to the distribution as libmygcc.a
#

if test "@GXX@" = "yes"
then
  cd $BASE/lib
  gcclib=`@CC@ --print-libgcc-file`
  if test $? -ne 0
  then
    print "Warning: Couldn't find libgcc.a!"
  else
    cp -p $gcclib libmygcc.a
  fi
  cd $SOURCE
fi

#if we are debugging, do not do tar/gz
if [ x$DEBUG = x1 ] ; then
 exit
fi

# This is needed to prefere gnu tar instead of tar because tar can't
# always handle long filenames

PATH_DIRS=`echo $PATH | sed -e 's/^:/. /' -e 's/:$/ ./' -e 's/::/ . /g' -e 's/:/ /g' `
which_1 ()
{
  for cmd
  do
    for d in $PATH_DIRS
    do
      for file in $d/$cmd
      do
	if test -x $file -a ! -d $file
	then
	  echo $file
	  exit 0
	fi
      done
    done
  done
  exit 1
}

#
# Create the result tar file
#

tar=`which_1 gtar`
if test "$?" = "1" -o "$tar" = ""
then
  tar=tar
fi

echo "Using $tar to create archive"
cd $TMP
$tar cvf $SOURCE/$NEW_NAME.tar $NEW_NAME
cd $SOURCE
echo "Compressing archive"
gzip -9 $NEW_NAME.tar
echo "Removing temporary directory"
rm -rf $BASE

echo "$NEW_NAME.tar.gz created"
