#!/bin/sh

# Copyright (C) 2003, 2005 MySQL AB
#  All rights reserved. Use is subject to license terms.
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

rm $1
touch $1
echo "<table border="1" width=640>" >> $1
echo "<tr>" >> $1
echo "<td><b>Name</b></td><td>&nbsp;</td><td width="70%"><b>Description</b></td>" >> $1
echo "</tr>" >> $1
testBasic --print_html >> $1
testBackup --print_html >> $1
testBasicAsynch --print_html >> $1
testDict --print_html >> $1
testBank --print_html >> $1
testIndex --print_html >> $1
testNdbApi --print_html >> $1
testNodeRestart --print_html >> $1
testOperations --print_html >> $1
testRestartGci --print_html >> $1
testScan --print_html >> $1
testScanInterpreter --print_html >> $1
testSystemRestart --print_html >> $1
echo "</table>" >> $1

