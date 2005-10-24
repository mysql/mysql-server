#!/bin/sh
# Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB
# 
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

# This script reports various configuration settings that may be needed
# when using the MySQL client library.

which ()
{
  IFS="${IFS=   }"; save_ifs="$IFS"; IFS=':'
  for file
  do
    for dir in $PATH
    do
      if test -f $dir/$file
      then
        echo "$dir/$file"
        continue 2
      fi
    done
    echo "which: no $file in ($PATH)"
    exit 1
  done
  IFS="$save_ifs"
}

#
# If we can find the given directory relatively to where mysql_config is
# we should use this instead of the incompiled one.
# This is to ensure that this script also works with the binary MySQL
# version

fix_path ()
{
  var=$1
  shift
  for filename
  do
    path=$basedir/$filename
    if [ -d "$path" ] ;
    then
      eval "$var"=$path
      return
    fi
  done
}

get_full_path ()
{
  file=$1

  # if the file is a symlink, try to resolve it
  if [ -h $file ];
  then
    file=`ls -l $file | awk '{ print $NF }'`
  fi

  case $file in
    /*) echo "$file";;
    */*) tmp=`pwd`/$file; echo $tmp | sed -e 's;/\./;/;' ;;
    *) which $file ;;
  esac
}

me=`get_full_path $0`

basedir=`echo $me | sed -e 's;/bin/mysql_config;;'`

ldata='@localstatedir@'
execdir='@libexecdir@'
bindir='@bindir@'
pkglibdir='@pkglibdir@'
fix_path pkglibdir lib/mysql lib
pkgincludedir='@pkgincludedir@'
fix_path pkgincludedir include/mysql include
version='@VERSION@'
socket='@MYSQL_UNIX_ADDR@'
port='@MYSQL_TCP_PORT@'
ldflags='@LDFLAGS@'

# Create options

libs="$ldflags -L$pkglibdir -lmysqlclient @ZLIB_DEPS@ @NON_THREADED_LIBS@"
libs="$libs @openssl_libs@ @STATIC_NSS_FLAGS@"
libs=`echo "$libs" | sed -e 's;  \+; ;g' | sed -e 's;^ *;;' | sed -e 's; *\$;;'`

libs_r="$ldflags -L$pkglibdir -lmysqlclient_r @ZLIB_DEPS@ @LIBS@ @openssl_libs@"
libs_r=`echo "$libs_r" | sed -e 's;  \+; ;g' | sed -e 's;^ *;;' | sed -e 's; *\$;;'`
cflags="-I$pkgincludedir @CFLAGS@ " #note: end space!
include="-I$pkgincludedir"
embedded_libs="$ldflags -L$pkglibdir -lmysqld @ZLIB_DEPS@ @LIBS@ @WRAPLIBS@ @innodb_system_libs@"
embedded_libs=`echo "$embedded_libs" | sed -e 's;  \+; ;g' | sed -e 's;^ *;;' | sed -e 's; *\$;;'`

# Remove some options that a client doesn't have to care about
# FIXME until we have a --cxxflags, we need to remove -Xa
#       and -xstrconst to make --cflags usable for Sun Forte C++
for remove in DDBUG_OFF DSAFEMALLOC USAFEMALLOC DSAFE_MUTEX \
              DPEDANTIC_SAFEMALLOC DUNIV_MUST_NOT_INLINE DFORCE_INIT_OF_VARS \
              DEXTRA_DEBUG DHAVE_purify 'O[0-9]' 'W[-A-Za-z]*' \
              Xa xstrconst
do
  # The first option we might strip will always have a space before it because
  # we set -I$pkgincludedir as the first option
  cflags=`echo "$cflags"|sed -e "s/ -$remove  */ /g"` 
done
cflags=`echo "$cflags"|sed -e 's/ *\$//'` 

usage () {
        cat <<EOF
Usage: $0 [OPTIONS]
Options:
        --cflags         [$cflags]
        --include        [$include]
        --libs           [$libs]
        --libs_r         [$libs_r]
        --socket         [$socket]
        --port           [$port]
        --version        [$version]
        --libmysqld-libs [$embedded_libs]
EOF
        exit 1
}

if test $# -le 0; then usage; fi

while test $# -gt 0; do
        case $1 in
        --cflags)  echo "$cflags" ;;
        --include) echo "$include" ;;
        --libs)    echo "$libs" ;;
        --libs_r)  echo "$libs_r" ;;
        --socket)  echo "$socket" ;;
        --port)    echo "$port" ;;
        --version) echo "$version" ;;
        --embedded-libs | --embedded | --libmysqld-libs) echo "$embedded_libs" ;;
        *)         usage ;;
        esac

        shift
done

#echo "ldata: '"$ldata"'"
#echo "execdir: '"$execdir"'"
#echo "bindir: '"$bindir"'"
#echo "pkglibdir: '"$pkglibdir"'"
#echo "pkgincludedir: '"$pkgincludedir"'"
#echo "version: '"$version"'"
#echo "socket: '"$socket"'"
#echo "port: '"$port"'"
#echo "ldflags: '"$ldflags"'"
#echo "client_libs: '"$client_libs"'"

exit 0
