#! /bin/sh

# Copyright (C) 2004 MySQL AB
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
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

# 1 - Dir
# 2 - Link dst

if [ $# -lt 1 ]
then
    exit 0
fi

files=`find $1 -type l -maxdepth 1`
res=$?
if [ $res -ne 0 ] || [ "$files" = "" ]
then
    exit 0
fi

rm -f $files



