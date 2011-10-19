# include/socket_event.inc
#
# Auxiliary routine running
# - some statement in connection con1
# or
# - connect/disconnect
# $loop_rounds times and checking if the changes to values caused by the action
# are reasonable.
#
# Requirements:
# 1.  Have socket_summary_by_instance_func running
# 2a. Have a connection con1
#     @con1_object_instance_begin needs to be the OBJECT_INSTANCE_BEGIN
#     value of the "client_connction" entry belonging to con1 within
#     socket_summary_by_instance.
#     $statement needs to contain the statement to be executed by con1.
# or
# 2b. Have assigned values to the following variables
#        $connect_host $connect_db $connect_user
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
   let $statement= Connect (con*,$connect_host,$connect_user,,$connect_db,,);
   # Some statements fail with ER_ACCESS_DENIED_ERROR
   --disable_abort_on_error
   --connect (con$loop_round,$connect_host,$connect_user,,$connect_db,,)
   --enable_abort_on_error
   let $my_errno= $mysql_errno;
   if(!$my_errno)
   {
      # Note(mleich):
      #    We are aware that this additional statement is overhead.
      #    But it ensures that SUM_NUMBER_OF_BYTES_READ and
      #    SUM_NUMBER_OF_BYTES_WRITE are updated.
      #    And this avoids the instabilities found when running
      #    the connect without this additional statement.
      DO 1;
   }
   --connection default
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
   --connection default
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
#    short life entry in socket_summary_by_instance -> wait till it doesn't exist
# 2. Connect with success ($my_errno = 0)
#    entry in performance_schema.threads -> wait_till_sleep.inc can be used
#    entry in socket_summary_by_instance -> wait till it does exist
# 3. SQL command failed ($my_errno <> 0)
#    entry in performance_schema.threads -> wait_till_sleep.inc can be used
if($is_connect)
{
   let $part=
   FROM performance_schema.socket_summary_by_instance
   WHERE EVENT_NAME LIKE '%client_connection'
     AND OBJECT_INSTANCE_BEGIN <> @default_object_instance_begin;

   if(!$my_errno)
   {
      # Wait till the new connection is visible in performance_schema.threads
      # and processlist_command is 'Sleep'.
      --source ../include/wait_till_sleep.inc

      # A successful connect causes that a new second row in
      # performance_schema.socket_summary_by_instance shows up.
      # Wait till this row is there.
      let $wait_timeout= 10;
      let $wait_condition=
      SELECT COUNT(*) = 1
      $part;
      --source include/wait_condition.inc
      if (!$success)
      {
         --echo # Error: We did not reach the expected state where a new
         --echo #        row in socket_summary_by_instance is visible
         eval
         SELECT *
         $part;
         --echo # abort
         exit;
      }
   }
   if($my_errno)
   {
      # Experiments with high parallel load showed that there is a very
      # period of time where a "client_connection" entry for a failing
      # Connect is visible.
      # We hope that sleep 1 is long enough so that PERFORMANCE_SCHEMA
      # can remove this row before we collect the after action state.
      let $wait_timeou= 5;
      let $wait_condition=
      SELECT COUNT(*) = 0
      $part;
      --source include/wait_condition.inc
      if(!$success)
      {
         --echo # Error: We did not reach the expected state.
         --echo #        A failing connect causes a "client_connection" entry
         --echo #        within socket_summary_by_instance having an extreme 
         --echo #        short lifetime.
         --echo #        This entry must have now disappeared.
         eval
         SELECT *
         $part;
         --echo # abort
         exit;
      }
   }
   # --sleep 3
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
      --disconnect con$loop_round
      # Wait till the connection using the DB = 'mysqltest' or
      # 'mysqlsupertest' has disappeared from performance_schema.threads
      let $part=
      FROM performance_schema.threads
      WHERE processlist_db IN ('mysqltest','mysqlsupertest');
      let $wait_timeout= 10;
      let $wait_condition=
      SELECT COUNT(*) = 0
      $part;
      --source include/wait_condition.inc
      if (!$success)
      {
         --echo # Error: The disconnect of the connection with processlist_db
         --echo #        IN ('mysqltest','mysqlsupertest') failed
         SELECT *
         $part;
         --echo # abort
         exit;
      }
      # Wait in addition till the corresponding 'client_connection' entry of
      # the connection using the DB = 'mysqltest' or 'mysqlsupertest' has disappeared.
      let $part=
      FROM performance_schema.socket_summary_by_instance
      WHERE EVENT_NAME LIKE '%client_connection'
        AND OBJECT_INSTANCE_BEGIN <> @default_object_instance_begin;
      let $wait_timeout= 10;
      let $wait_condition=
      SELECT COUNT(*) = 0
      $part;
      --source include/wait_condition.inc
      if (!$success)
      {
         --echo # Error: The entry of the disconnectd connection with processlist_db
         --echo #        IN ('mysqltest','mysqlsupertest') did not disappear
         SELECT *
         $part;
         --echo # abort
         exit;
      }
   }
   # --sleep 3
}
inc $loop_round;

}

--enable_query_log
--enable_result_log

