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

# include/socket_event.inc
#
# Auxiliary routine
# - running some statement in connection con1
#   and checking the changes for the client_connction" entry belonging to con1
#   within socket_summary_by_instance and
# - checking if the changes to values caused by the statement execution are
#   reasonable and stable
#
# Requirements:
# 1. Have socket_summary_by_instance_func running
# 2. Have a connection con1
#    @con1_object_instance_begin needs to be the OBJECT_INSTANCE_BEGIN
#    value of the "client_connction" entry belonging to con1 within
#    socket_summary_by_instance.
# 3. $statement needs to contain the statement to be executed by con1.
#

let $my_errno= 0;

let $loop_round= 1;
while($loop_round <= $loop_rounds)
{

--disable_query_log

# Collect the current state
#==========================
eval $truncate;
eval $insert_before;

# Run the operation
#==================
if($is_connect)
{
   let $statement= Connect (con1,$connect_host,$connect_user,,$connect_db,,);
   # Some statements fail with ER_ACCESS_DENIED_ERROR
   --disable_abort_on_error
   --connect (con1,$connect_host,$connect_user,,$connect_db,,)
   --enable_abort_on_error
   let $my_errno= $mysql_errno;
}
if(!$is_connect)
{
   --connection con1
   # Print the statement outcome once.
   if($loop_round == 1)
   {
   --enable_query_log
   --enable_result_log
   --horizontal_results
   }
   # One of the statements to be checked is expected to fail with ER_NO_SUCH_TABLE.
   --disable_abort_on_error
   eval $statement;
   --enable_abort_on_error
   --disable_query_log
   --disable_result_log
}

# Wait till the operation is really finished. We expect that there will be no
# changes to the statistics of the additional connection after this point of time.
#=================================================================================
--connection default
# Variants:
#----------
# 1. Connect failed ($my_errno <> 0) 
#    no entry in performance_schema.threads -> wait_till_sleep.inc cannot be used
#    Just 3 seconds sleep
# 2. Connect with success ($my_errno = 0) 
#    entry in performance_schema.threads -> wait_till_sleep.inc can be used
#    Additional 3 second sleep
# 3. SQL command failed ($my_errno <> 0)
#    entry in performance_schema.threads -> wait_till_sleep.inc can be used
if($is_connect)
{
   if(!$my_errno)
   {
      --source ../include/wait_till_sleep.inc
   }
   if($my_errno)
   {
      --sleep 6
   }
}
if(!$is_connect)
{
   --source ../include/wait_till_sleep.inc
}

# Various checks
#===============
# 1. Check statistics in general
#-------------------------------
# ../include/socket_summary_check.inc also inserts the 'After' state into
# mysqltest.my_socket_summary_by_instance.
--source ../include/socket_summary_check.inc
--disable_query_log
--disable_result_log

if($is_connect)
{
   eval $get_object_instance_begin;
   eval $insert_pseudo_before;
}

eval $insert_delta;
# Correct the values of the columns statement and run
eval
UPDATE mysqltest.socket_summary_by_instance_detail
SET statement = '$statement'
WHERE statement IS NULL;
eval
UPDATE mysqltest.socket_summary_by_instance_detail
SET run = $loop_round
WHERE run IS NULL;

if($is_connect)
{
   # Only in case the connect was successful ($my_errno = 0) than we have to disconnect.
   if(!$my_errno)
   {
      --disconnect con1
      # Wait till the connection using the DB = 'mysqltest' or
      # 'mysqlsupertest' has disappeared
      let $wait_timeout= 5;
      let $wait_condition=
      SELECT COUNT(*) = 0
      FROM performance_schema.threads
      WHERE processlist_db IN ('mysqltest','mysqlsupertest');
      --source include/wait_condition.inc
      if (!$success)
      {
         --echo # Error: The disconnect of the connection with processlist_db
         --echo #        IN ('mysqltest','mysqlsupertest') failed
         SELECT * FROM performance_schema.threads
         WHERE processlist_db IN ('mysqltest','mysqlsupertest');
         --echo # abort
         exit;
      }
   }
   --sleep 3
}
inc $loop_round;

}

--enable_query_log
--enable_result_log

