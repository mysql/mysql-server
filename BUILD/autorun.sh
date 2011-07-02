#!/bin/sh

# Copyright (C) 2005 MySQL AB
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU Library General Public
# License as published by the Free Software Foundation; version 2
# of the License.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Library General Public License for more details.
#
# You should have received a copy of the GNU Library General Public
# License along with this library; if not, write to the Free
# Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
# MA 02111-1307, USA

# Create MySQL cmake configure wrapper

die() { echo "$@"; exit 1; }

# Use a configure script that will call CMake.
path=`dirname $0`
cp $path/cmake_configure.sh $path/../configure
chmod +x $path/../configure
