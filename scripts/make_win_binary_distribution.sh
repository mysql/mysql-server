#!/bin/sh

#
# Script to create a Windows binary package
#
# This is intended to be used under Cygwin, and will generate
# an archive named in the form mysql<suffix>-<version>-noinstall.zip

version=@VERSION@

DEBUG=0
SUFFIX=""
DIRNAME=""
EXTRA=""

#
# This script must run from MySQL top directory
#

if [ ! -f scripts/make_win_binary_distribution ]; then
  echo "ERROR : You must run this script from the MySQL top-level directory"
  exit 1
fi

#
# Debug print of the status
#

print_debug()
{
  for statement
  do
    if [ "$DEBUG" = "1" ] ; then
      echo $statement
    fi
  done
}

#
# Usage of the script
#

show_usage()
{
  echo "MySQL utility script to create a Windows binary package"
  echo ""
  echo "This is intended to be used under Cygwin, and will generate"
  echo "an archive named in the form mysql<suffix>-<version>-noinstall.zip"
  echo "Takes the following arguments:"
  echo ""
  echo "  --dirname  Directory to use for copying files"
  echo "  --extra    Directory to get extra files from"
  echo "  --suffix   Name to append to 'mysql' for this binary"
  echo "  --help     Show this help message"
  exit 0
}

#
# Parse the input arguments
#

parse_arguments() {
  for arg do
    case "$arg" in
      --debug)    DEBUG=1;;
      --extra=*) EXTRA=`echo "$arg" | sed -e "s;--extra=;;"` ;;
      --suffix=*) SUFFIX=`echo "$arg" | sed -e "s;--suffix=;;"` ;;
      --dirname=*) DIRNAME=`echo "$arg" | sed -e "s;--dirname=;;"` ;;
      --help)     show_usage ;;
      *)
  echo "Unknown argument '$arg'"
  exit 1
      ;;
    esac
  done
}

parse_arguments "$@"

if [ -z "$DIRNAME" ]; then
  $DIRNAME="dist"
fi

print_debug "Making directories"
mkdir $DIRNAME
$DIRNAME="$DIRNAME/mysql-$version"
mkdir $DIRNAME

for dir in bin lib lib/opt lib/debug Embedded Embedded/DLL Embedded/DLL/debug Embedded/DLL/release Embedded/static Embedded/static/release examples examples/libmysqltest
do
  mkdir $DIRNAME/$dir
done

if [ $EXTRA ]; then
  print_debug "Copying extra files"
  cp -fr $EXTRA/* $DIRNAME
fi

# Dirs to be copied as-is
for dir in data Docs include scripts share
do
  print_debug "Copying $dir to $DIRNAME/"
  cp -fr $dir $DIRNAME
done

print_debug "Copying tests to $DIRNAME/examples/"
cp -fr tests $DIRNAME/examples

print_debug "Copying sql-bench to $DIRNAME/bench"
mkdir $DIRNAME/bench
cp -fr sql-bench/* $DIRNAME/bench

print_debug "Copying mysql-test to $DIRNAME/mysql-test"
mkdir $DIRNAME/mysql-test
cp -fr mysql-test/* $DIRNAME/mysql-test

print_debug "Copying support-files to $DIRNAME"
cp support-files/* $DIRNAME

# Files for bin
for i in client_release/* client_debug/mysqld.exe lib_release/libmySQL.dll
do
  print_debug "Copying $i to $DIRNAME/bin"
  cp $i $DIRNAME/bin
done

# Files for include
for i in libmysql/libmysql.def libmysqld/libmysqld.def
do
  print_debug "Copying $i to $DIRNAME/include"
  cp $i $DIRNAME/include
done

# Windows users are used to having dbug.h ?
cp include/my_dbug.h $DIRNAME/include/dbug.h

# Libraries found in lib_release and lib_debug
for i in libmySQL.dll libmysql.lib zlib.lib mysqlclient.lib mysys.lib regex.lib strings.lib
do
  print_debug "Copying lib_release/$i to $DIRNAME/lib/opt"
  cp lib_release/$i $DIRNAME/lib/opt
  print_debug "Copying lib_debug/$i to $DIRNAME/lib/debug"
  cp lib_debug/$i $DIRNAME/lib/debug
done

print_debug "Copying lib_release/mysys-max.lib to $DIRNAME/lib/opt"
cp lib_release/mysys-max.lib $DIRNAME/lib/opt

# Embedded server
for i in libmysqld.dll libmysqld.lib libmysqld.exp
do
  print_debug "Copying lib_release/$i to $DIRNAME/Embedded/DLL/release"
  cp lib_release/$i $DIRNAME/Embedded/DLL/release
  print_debug "Copying lib_debug/$i to $DIRNAME/Embedded/DLL/debug"
  cp lib_debug/$i $DIRNAME/Embedded/DLL/debug
done

# Static embedded
print_debug "Copying lib_release/mysqlserver.lib to $DIRNAME/Embedded/static/release"
cp lib_release/mysqlserver.lib $DIRNAME/Embedded/static/release

# libmysqltest
for i in mytest.c mytest.dsp mytest.dsw mytest.exe
do
  print_debug "Copying libmysqltest/release/$i to $DIRNAME/examples/libmysqltest"
  cp libmysqltest/release/$i $DIRNAME/examples/libmysqltest
done

print_debug "Copying README.txt"
cp README.txt $DIRNAME

if [ -f MySQLEULA.txt ]; then
  print_debug "Commercial version: copying MySQLEULA.txt"
  cp MySQLEULA.txt $DIRNAME
  rm $DIRNAME/Docs/COPYING
else
  print_debug "GPL version: copying COPYING"
  cp Docs/COPYING $DIRNAME
fi

print_debug "Invoking zip to package the binary"
zip -r mysql$SUFFIX-$version-win-noinstall.zip $DIRNAME

print_debug "Deleting intermediate directory"
rm -rf $DIRNAME
