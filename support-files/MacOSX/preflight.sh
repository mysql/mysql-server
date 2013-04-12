#!/bin/sh

# Copyright (C) 2003, 2005 MySQL AB
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
# Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
# MA 02110-1301, USA

#
# preflight - this script will be executed before the MySQL PKG
# installation will be performed.
#
# If this package has been compiled with a prefix ending with "mysql" (e.g.
# /usr/local/mysql or /opt/mysql), it will rename any previously existing
# directory with this name before installing the new package (which includes
# a symlink named "mysql", pointing to the newly installed directory, which
# is named mysql-<version>)
#

PREFIX="@prefix@"
BASENAME=`basename $PREFIX`

if [ -d $PREFIX -a ! -L $PREFIX -a $BASENAME = "mysql" ] ; then
	mv $PREFIX $PREFIX.bak
fi
