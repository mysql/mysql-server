#!/bin/sh
# Copyright (c) 2000, 2014, Oracle and/or its affiliates. All rights reserved.
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
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

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

# Script might have been renamed but assume mysql_<something>config<something>
basedir=`echo $me | sed -e 's;/bin/mysql_.*config.*;;'`

ldata='@localstatedir@'
execdir='@libexecdir@'
bindir='@bindir@'

# If installed, search for the compiled in directory first (might be "lib64")
pkglibdir='@pkglibdir@'
pkglibdir_rel=`echo $pkglibdir | sed -e "s;^$basedir/;;"`
fix_path pkglibdir $pkglibdir_rel @libsubdir@/mysql @libsubdir@

plugindir='@pkgplugindir@'
plugindir_rel=`echo $plugindir | sed -e "s;^$basedir/;;"`
fix_path plugindir $plugindir_rel @libsubdir@/mysql/plugin @libsubdir@/plugin

pkgincludedir='@pkgincludedir@'
if [ -f "$basedir/include/mysql/mysql.h" ]; then
  pkgincludedir="$basedir/include/mysql"
elif [ -f "$basedir/include/mysql.h" ]; then
  pkgincludedir="$basedir/include"
fi

version='@VERSION@'
socket='@MYSQL_UNIX_ADDR@'
ldflags='@LDFLAGS@'

if [ @MYSQL_TCP_PORT_DEFAULT@ -eq 0 ]; then
  port=0
else
  port=@MYSQL_TCP_PORT@
fi

# Create options 
# We intentionally add a space to the beginning and end of lib strings, simplifies replace later
libs=" $ldflags -L$pkglibdir @RPATH_OPTION@ -lmysqlclient @ZLIB_DEPS@ @NON_THREADED_LIBS@"
libs="$libs @openssl_libs@ @STATIC_NSS_FLAGS@ "
libs_r=" $ldflags -L$pkglibdir @RPATH_OPTION@ -lmysqlclient_r @ZLIB_DEPS@ @CLIENT_LIBS@ @openssl_libs@ "
embedded_libs=" $ldflags -L$pkglibdir @RPATH_OPTION@ -lmysqld @LIBDL@ @ZLIB_DEPS@ @LIBS@ @WRAPLIBS@ @openssl_libs@ "
embedded_libs="$embedded_libs @QUOTED_CMAKE_CXX_LINK_FLAGS@"

cflags="-I$pkgincludedir @CFLAGS@ " #note: end space!
cxxflags="-I$pkgincludedir @CXXFLAGS@ " #note: end space!
include="-I$pkgincludedir"

# Remove some options that a client doesn't have to care about
for remove in DDBUG_OFF DSAFE_MUTEX DFORCE_INIT_OF_VARS \
              DEXTRA_DEBUG DHAVE_purify O 'O[0-9]' 'xO[0-9]' 'W[-A-Za-z]*' \
              'mtune=[-A-Za-z0-9]*' 'mcpu=[-A-Za-z0-9]*' 'march=[-A-Za-z0-9]*' \
              unroll2 ip mp restrict
do
  # The first option we might strip will always have a space before it because
  # we set -I$pkgincludedir as the first option
  cflags=`echo "$cflags"|sed -e "s/ -$remove  */ /g"` 
  cxxflags=`echo "$cxxflags"|sed -e "s/ -$remove  */ /g"` 
done
cflags=`echo "$cflags"|sed -e 's/ *\$//'` 
cxxflags=`echo "$cxxflags"|sed -e 's/ *\$//'` 

# Same for --libs(_r)
for remove in lmtmalloc static-libcxa i-static static-intel
do
  # We know the strings starts with a space
  libs=`echo "$libs"|sed -e "s/ -$remove  */ /g"` 
  libs_r=`echo "$libs_r"|sed -e "s/ -$remove  */ /g"` 
  embedded_libs=`echo "$embedded_libs"|sed -e "s/ -$remove  */ /g"` 
done

# Strip trailing and ending space if any, and '+' (FIXME why?)
libs=`echo "$libs" | sed -e 's;  \+; ;g' | sed -e 's;^ *;;' | sed -e 's; *\$;;'`
libs_r=`echo "$libs_r" | sed -e 's;  \+; ;g' | sed -e 's;^ *;;' | sed -e 's; *\$;;'`
embedded_libs=`echo "$embedded_libs" | sed -e 's;  \+; ;g' | sed -e 's;^ *;;' | sed -e 's; *\$;;'`

usage () {
        cat <<EOF
Usage: $0 [OPTIONS]
Options:
        --cflags         [$cflags]
        --cxxflags       [$cxxflags]
        --include        [$include]
        --libs           [$libs]
        --libs_r         [$libs_r]
        --plugindir      [$plugindir]
        --socket         [$socket]
        --port           [$port]
        --version        [$version]
        --libmysqld-libs [$embedded_libs]
        --variable=VAR   VAR is one of:
                pkgincludedir [$pkgincludedir]
                pkglibdir     [$pkglibdir]
                plugindir     [$plugindir]
EOF
        exit 1
}

if test $# -le 0; then usage; fi

while test $# -gt 0; do
        case $1 in
        --cflags)  echo "$cflags" ;;
        --cxxflags)echo "$cxxflags";;
        --include) echo "$include" ;;
        --libs)    echo "$libs" ;;
        --libs_r)  echo "$libs_r" ;;
        --plugindir) echo "$plugindir" ;;
        --socket)  echo "$socket" ;;
        --port)    echo "$port" ;;
        --version) echo "$version" ;;
        --embedded-libs | --embedded | --libmysqld-libs) echo "$embedded_libs" ;;
        --variable=*)
          var=`echo "$1" | sed 's,^[^=]*=,,'`
          case "$var" in
            pkgincludedir) echo "$pkgincludedir" ;;
            pkglibdir) echo "$pkglibdir" ;;
            plugindir) echo "$plugindir" ;;
            *) usage ;;
          esac
          ;;
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
