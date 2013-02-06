#!/bin/sh

# Copyright (c) 2004, 2012, Oracle and/or its affiliates. All rights reserved.
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

#
# This script's purpose is to prepare for a subsequent 'make dist';
# We don't actually have to compile anything,
# as the 'dist' target in our .cmake files is self-contained
#   i.e. it generates source dependencies.
#

path=`dirname $0`
. $path/autorun.sh

# By default we get the "community" feature set from
#    cmake/build_configurations/feature_set.cmake
#
./configure
