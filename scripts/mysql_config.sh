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

fix_path ()
{
  eval path=\$"$1"
  if [ ! -f "$path" ] ;
  then
   eval "$1"=$basedir/$2
  fi
}

abs_path=`expr \( substr $0 1 1 \) = '/'`
if [ "x$abs_path" = "x1" ] ; then
 me=$0
else 
 me=`which mysql_config`
fi

basedir=`echo $me | sed -e 's;/bin/mysql_config;;'`

ldata='@localstatedir@'
execdir='@libexecdir@'
bindir='@bindir@'
pkglibdir='@pkglibdir@'
fix_path pkglibdir lib/mysql
pkgincludedir='@pkgincludedir@'
fix_path pkgincludedir include/mysql
version='@VERSION@'
socket='@MYSQL_UNIX_ADDR@'
port='@MYSQL_TCP_PORT@'
ldflags='@LDFLAGS@'
client_libs='@CLIENT_LIBS@'

libs="$ldflags -L'$pkglibdir' -lmysqlclient $client_libs"
cflags="-I'$pkgincludedir'"

usage () {
        cat <<EOF
Usage: $0 [OPTIONS]
Options:
        --cflags        [$cflags]
        --libs          [$libs]
        --socket        [$socket]
        --port          [$port]
        --version       [$version]
EOF
        exit 1
}

if test $# -le 0; then usage; fi

while test $# -gt 0; do
        case $1 in
        --cflags)  echo "$cflags" ;;
        --libs)    echo "$libs" ;;
        --socket)  echo "$socket" ;;
        --port)    echo "$port" ;;
        --version) echo "$version" ;;
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
