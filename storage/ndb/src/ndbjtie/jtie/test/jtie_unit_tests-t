#!/bin/sh

# Copyright (c) 2010, Oracle and/or its affiliates. All rights reserved.
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
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

# see: The MySQL Test Framework :: 7 Creating and Executing Unit Tests
#   (MySQL Cluster Documentation)/mysqltest/en/unit-test.html
# see: Test Anything Protocol (TAP)
#   http://testanything.org/wiki/index.php/Main_Page
#   http://en.wikipedia.org/wiki/Test_Anything_Protocol

# not sure which protocol version we're using
#echo "TAP version 13"

# test range
echo "1..3"

script_dir=`dirname $0`

# log file for output from this script
log="jtie_unit_tests-t.log"
rm -f "$log"
touch "$log"

test_counter=0

#
# Run a simpler shell script test, and report it as TAP
#
# Arguments:
#   shell script hame
#
run_test()
{
    test_name=$1;
    test_counter=`expr $test_counter + 1`
    script_name="$script_dir/$test_name/test_$test_name.sh"

    echo "running test '$script_name':" >> "$log" 2>&1
    if [ ! -x "$script_name" ]; then
       status="ok $test_counter # skip $test_name test file missing"
    else
      ./$script_name >> "$log" 2>&1
      s=""
      if [ "$?" -ne "0" ]; then
        s="not "
      fi
      status="${s}ok $test_counter - $test_name"
    fi;
    echo "$status" >> "$log" 2>&1
    echo "" >> "$log" 2>&1
    echo "$status"
    cd ..
}

run_test "myapi"
run_test "myjapi"
run_test "unload"

echo "done." >> "$log" 2>&1
