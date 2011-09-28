# Copyright (c) 2011, Oracle and/or its affiliates. All rights reserved.
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; version 2 of
# the License.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
# 02110-1301  USA
#

# Wait till the action of the connection using the DB = 'mysqltest' or
# 'mysqlsupertest' is finished ( Command = 'Sleep').

let $wait_timeout= 10;
let $wait_condition=
SELECT COUNT(*) = 1
FROM performance_schema.threads
WHERE processlist_db IN ('mysqltest','mysqlsupertest')
  AND processlist_command = 'Sleep';
--source include/wait_condition.inc
if (!$success)
{
   --echo # Error: We did not reach the expected state where processlist_command = 'Sleep'
   SELECT * FROM performance_schema.threads
   WHERE processlist_db IN ('mysqltest','mysqlsupertest');
   --echo # abort
   exit;
}

