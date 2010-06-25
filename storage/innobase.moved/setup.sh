#!/bin/sh
#
# Copyright (c) 1995, 2009, Innobase Oy. All Rights Reserved.
# 
# This program is free software; you can redistribute it and/or modify it under
# the terms of the GNU General Public License as published by the Free Software
# Foundation; version 2 of the License.
# 
# This program is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License along with
# this program; if not, write to the Free Software Foundation, Inc., 59 Temple
# Place, Suite 330, Boston, MA 02111-1307 USA
#
# Prepare the MySQL source code tree for building
# with checked-out InnoDB Subversion directory.

# This script assumes that the current directory is storage/innobase.

set -eu

TARGETDIR=../storage/innobase

# link the build scripts
BUILDSCRIPTS="compile-innodb compile-innodb-debug"
for script in $BUILDSCRIPTS ; do
	ln -sf $TARGETDIR/$script ../../BUILD/
done

cd ../../mysql-test/t
ln -sf ../$TARGETDIR/mysql-test/*.test ../$TARGETDIR/mysql-test/*.opt .
cd ../r
ln -sf ../$TARGETDIR/mysql-test/*.result .
cd ../include
ln -sf ../$TARGETDIR/mysql-test/*.inc .

# Apply any patches that are needed to make the mysql-test suite successful.
# These patches are usually needed because of deviations of behavior between
# the stock InnoDB and the InnoDB Plugin.
cd ../..
for patch in storage/innobase/mysql-test/patches/*.diff ; do
	if [ "${patch}" != "storage/innobase/mysql-test/patches/*.diff" ] ; then
		patch -p0 < ${patch}
	fi
done
