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

ldata='@localstatedir@'
execdir='@libexecdir@'
bindir='@bindir@'
pkglibdir='@pkglibdir@'
pkgincludedir='@pkgincludedir@'
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

if ! test $# -gt 0; then usage; fi

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
