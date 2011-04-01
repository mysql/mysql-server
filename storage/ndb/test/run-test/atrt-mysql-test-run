#!/bin/sh

# Copyright (C) 2004, 2005 MySQL AB
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

set -x
p=`pwd`
cd $MYSQL_BASE_DIR/mysql-test 
./mysql-test-run --with-ndbcluster --ndb-connectstring=$NDB_CONNECTSTRING $* | tee $p/output.txt

f=`grep -c '\[ fail \]' $p/output.txt`
o=`grep -c '\[ pass \]' $p/output.txt`

if [ $o -gt 0 -a $f -eq 0 ]
then
    echo "NDBT_ProgramExit: OK"
    exit 0
fi

echo "NDBT_ProgramExit: Failed"
exit 1
