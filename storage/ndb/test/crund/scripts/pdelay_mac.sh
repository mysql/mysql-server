#!/bin/bash

# Copyright (c) 2013, 2022, Oracle and/or its affiliates.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0,
# as published by the Free Software Foundation.
#
# This program is also distributed with certain software (including
# but not limited to OpenSSL) that is licensed under separate terms,
# as designated in a particular file or component or in included license
# documentation.  The authors of MySQL hereby grant you an additional
# permission to link the program and your derivative works with the
# separately licensed software that they have included with MySQL.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License, version 2.0, for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

# simulate network latency on Mac OS X, see man ipfw(8)

#set -x

if [ $# -lt 1 ] ; then
  echo "usage: `basename $0` <delay-ms> [<port> ...]"
  echo "                 delay < 0 deletes the delay rule set"
  echo "                       = 0 disables the delay rule set"
  echo "                       > 0 enables the delay rule set"
  echo "                 port... adds the ports to the delay rule set"
  exit 1
fi

delay=$1
shift
ports=$*
#echo "simulate a delay of ($delay + $delay) ms on ports $ports"
echo "simulate a delay of $delay ms on destination ports $ports"
pipe=4711 # 1..64k
rset=21   # 0..30

if [ $delay -lt 0 ] ; then
    echo "removing rules (as admin)..."
    sudo ipfw delete set $rset
    sudo ipfw pipe delete $pipe > /dev/null 2>&1
elif [ $delay -eq 0 ] ; then
    echo "disabling rules (as admin)..."
    sudo ipfw set disable $rset
else
    echo "adding rules (as admin)..."
    # not strictly necessary but better change atomically
    sudo ipfw set disable $rset
    sudo ipfw pipe $pipe config delay $delay
    sudo ipfw set enable $rset
    # rules not visible if set disabled
    for p in $ports ; do
        sudo ipfw list $p > /dev/null
        if [ $? -ne 0 ] ; then
            # not strictly necessary but better change atomically
            sudo ipfw set disable $rset
#            sudo ipfw add $p set $rset pipe $pipe src-port $p > /dev/null 2>&1
            sudo ipfw add $p set $rset pipe $pipe dst-port $p > /dev/null 2>&1
            sudo ipfw set enable $rset
        fi
    done
fi
echo "current rules:"
sudo ipfw list | grep $pipe
#sudo ipfw show
echo "done."

#set +x
