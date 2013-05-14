#!/bin/sh

# Copyright (c) 2004, 2005, 2007 MySQL AB

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
# You should have received a copy of the GNU Library General Public
# License along with this library; if not, write to the Free
# Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
# MA  02110-1301, USA


set -e

mkdir -p result
cd result
rm -rf *

while [ $# -gt 0 ]
do
  rsync -a --exclude='BACKUP' --exclude='ndb_*_fs' "$1" .
  shift
done

#
# clean tables...not to make results too large
#
lst=`find . -name '*.frm'`
if [ "$lst" ]
then
    for i in $lst
    do
	basename=`echo $i | sed 's!\.frm!!'`
	if [ "$basename" ]
	then
	    rm -f $basename.*
	fi
    done
fi
