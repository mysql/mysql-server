#!/bin/bash

# Copyright (c) 2013, 2024, Oracle and/or its affiliates.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0,
# as published by the Free Software Foundation.
#
# This program is designed to work with certain software (including
# but not limited to OpenSSL) that is licensed under separate terms,
# as designated in a particular file or component or in included license
# documentation.  The authors of MySQL hereby grant you an additional
# permission to link the program and your derivative works with the
# separately licensed software that they have either included with
# the program or referenced in the documentation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License, version 2.0, for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

# simulate network latency on data nodes' ports listed in config.ini

#set -x

if [ $# -lt 1 ] ; then
  echo "usage: `basename $0` <delay-ms> [<config.ini>]"
  exit 1
fi

delay=$1
myini=${2:-"../config.ini"}
if [ ! -e "$myini" ] ; then
  echo "file not found: $myini"
  exit 1
fi

ports="`grep '^ServerPort' $myini | sed -e 's/.*=//' -e 's/#.*//'`"
ports="`echo $ports`" # remove newlines
echo "found ServerPorts in $myini : $ports"

if [ x"`uname`" = x"Darwin" ] ; then
    ./pdelay_mac.sh $delay $ports
fi

#set +x
