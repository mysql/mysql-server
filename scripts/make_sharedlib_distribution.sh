#!/bin/sh
# Copyright (C) 2003-2004, 2006 MySQL AB
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

# The default path should be /usr/local

# Get some info from configure
# chmod +x ./scripts/setsomevars

machine=@MACHINE_TYPE@
system=@SYSTEM_TYPE@
version=@VERSION@
export machine system version
SOURCE=`pwd` 
CP="cp -p"
MV="mv"

STRIP=1
DEBUG=0
SILENT=0
TMP=/tmp
SUFFIX=""

parse_arguments() {
  for arg do
    case "$arg" in
      --debug)    DEBUG=1;;
      --tmp=*)    TMP=`echo "$arg" | sed -e "s;--tmp=;;"` ;;
      --suffix=*) SUFFIX=`echo "$arg" | sed -e "s;--suffix=;;"` ;;
      --no-strip) STRIP=0 ;;
      --silent)   SILENT=1 ;;
      *)
	echo "Unknown argument '$arg'"
	exit 1
        ;;
    esac
  done
}

parse_arguments "$@"

BASE=$TMP/my_dist$SUFFIX

if [ -d $BASE ] ; then
 rm -r -f $BASE
fi

mkdir -p $BASE/lib

for i in \
  libmysql/.libs/libmysqlclient.so* \
  libmysql/.libs/libmysqlclient.sl* \
  libmysql/.libs/libmysqlclient*.dylib \
  libmysql_r/.libs/libmysqlclient_r.so* \
  libmysql_r/.libs/libmysqlclient_r.sl* \
  libmysql_r/.libs/libmysqlclient_r*.dylib
do
  if [ -f $i ]
  then
    $CP $i $BASE/lib
   fi
done

# Change the distribution to a long descriptive name
NEW_NAME=mysql-shared-$version-$system-$machine$SUFFIX
BASE2=$TMP/$NEW_NAME
rm -r -f $BASE2
mv $BASE $BASE2
BASE=$BASE2

#if we are debugging, do not do tar/gz
if [ x$DEBUG = x1 ] ; then
 exit
fi

# This is needed to prefer GNU tar instead of tar because tar can't
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

tar=`which_1 gnutar gtar`
if test "$?" = "1" -o "$tar" = ""
then
  tar=tar
fi

echo "Using $tar to create archive"
cd $TMP

OPT=cvf
if [ x$SILENT = x1 ] ; then
  OPT=cf
fi

$tar $OPT $SOURCE/$NEW_NAME.tar $NEW_NAME
cd $SOURCE
echo "Compressing archive"
gzip -9 $NEW_NAME.tar
echo "Removing temporary directory"
rm -r -f $BASE

echo "$NEW_NAME.tar.gz created"
