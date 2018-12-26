######## include/ddl3.inc ######
#
# Stress the storage engine with rapid CREATE/DROP TABLE/INDEX
# and following SELECT/INSERT/SHOW etc.
# Subtest 3 variants (3A - 3D)
#
# The variables
#     $loop_size -- number of rounds till we look at the clock again
#     $runtime   -- rough intended runtime per subtest variant
#     $engine_type -- storage engine to be used in CREATE TABLE
# must be set within the routine sourcing this script.
#
# Other stuff which must already exist:
# - connection con2
# - stmt_start and stmt_break prepared by the default connection
#
# Please look for more details within include/ddl2.inc.
#
# Creation of this test:
# 2007-07-04 mleich
#


#----------------------------------------------------------------------
# Settings for Subtest 3 variants
# Scenario: CREATE TABLE/CREATE TABLE(F)/DROP TABLE/DROP TABLE(F)
let $create_table=  CREATE TABLE t1 (f1 BIGINT NOT NULL) ENGINE=$engine_type;
let $drop_table=    DROP TABLE t1;
#----------------------------------------------------------------------

#
--echo # Subtest 3A (one connection, no PREPARE/EXECUTE)
--echo #    connection  action
--echo #    default:    $create_table
--echo #    default:    $create_table (expect to get ER_TABLE_EXISTS_ERROR)
--echo #    default:    $drop_table
--echo #    default:    $drop_table (expect to get ER_BAD_TABLE_ERROR)
--disable_query_log
--disable_result_log
connection default;
let $run= 1;
# Determine the current time.
EXECUTE stmt_start;
# Run execution loops till the planned runtime is reached
while ($run)
{
   let $loop_run= $loop_size;
   while ($loop_run)
   {
      eval $create_table;
      --error 0,ER_TABLE_EXISTS_ERROR
      eval $create_table;
      if (!$mysql_errno)
      {
         --echo # Error: CREATE TABLE was successful though we expected ER_TABLE_EXISTS_ERROR
         --echo # abort
         exit;
      }
      eval $drop_table;
      --error 0,ER_BAD_TABLE_ERROR
      eval $drop_table;
      if (!$mysql_errno)
      {
         --echo # Error: DROP TABLE was successful though we expected ER_BAD_TABLE_ERROR)
         --echo # abort
         exit;
      }
      dec $loop_run;
   }
   if (`EXECUTE stmt_break`)
   {
      let $run= 0;
   }
}
--enable_result_log
--enable_query_log
#
--echo # Subtest 3B (one connection, use PREPARE/EXECUTE)
--echo #    connection  action
--echo #    default:    $create_table
--echo #    default:    $create_table (expect to get ER_TABLE_EXISTS_ERROR)
--echo #    default:    $drop_table
--echo #    default:    $drop_table (expect to get ER_BAD_TABLE_ERROR)
--disable_query_log
--disable_result_log
connection default;
eval PREPARE create_table FROM "$create_table";
EXECUTE create_table;
eval PREPARE drop_table FROM "$drop_table";
EXECUTE drop_table;
let $run= 1;
# Determine the current time.
EXECUTE stmt_start;
# Run execution loops till the planned runtime is reached
while ($run)
{
   let $loop_run= $loop_size;
   while ($loop_run)
   {
      EXECUTE create_table;
      --error 0,ER_TABLE_EXISTS_ERROR
      EXECUTE create_table;
      if (!$mysql_errno)
      {
         --echo # Error: CREATE TABLE was successful though we expected ER_TABLE_EXISTS_ERROR
         --echo # abort
         exit;
      }
      EXECUTE drop_table;
      --error 0,ER_BAD_TABLE_ERROR
      EXECUTE drop_table;
      if (!$mysql_errno)
      {
         --echo # Error: DROP TABLE was successful though we expected ER_BAD_TABLE_ERROR)
         --echo # abort
         exit;
      }
      dec $loop_run;
   }
   if (`EXECUTE stmt_break`)
   {
      let $run= 0;
   }
}
DEALLOCATE PREPARE create_table;
DEALLOCATE PREPARE drop_table;
--enable_result_log
--enable_query_log
#
--echo # Subtest 3C (two connections, no PREPARE/EXECUTE)
--echo #    connection  action
--echo #    default:    $create_table
--echo #    con2:       $create_table (expect to get ER_TABLE_EXISTS_ERROR)
--echo #    default:    $drop_table
--echo #    con2:       $drop_table (expect to get ER_BAD_TABLE_ERROR)
--disable_query_log
--disable_result_log
connection default;
let $run= 1;
# Determine the current time.
EXECUTE stmt_start;
# Run execution loops till the planned runtime is reached
while ($run)
{
   let $loop_run= $loop_size;
   while ($loop_run)
   {
      eval $create_table;
      connection con2;
      --error 0,ER_TABLE_EXISTS_ERROR
      eval $create_table;
      if (!$mysql_errno)
      {
         --echo # Error: CREATE TABLE was successful though we expected ER_TABLE_EXISTS_ERROR
         --echo # abort
         exit;
      }
      connection default;
      eval $drop_table;
      connection con2;
      --error 0,ER_BAD_TABLE_ERROR
      eval $drop_table;
      if (!$mysql_errno)
      {
         --echo # Error: DROP TABLE was successful though we expected ER_BAD_TABLE_ERROR)
         --echo # abort
         exit;
      }
      connection default;
      dec $loop_run;
   }
   if (`EXECUTE stmt_break`)
   {
      let $run= 0;
   }
}
--enable_result_log
--enable_query_log
#
--echo # Subtest 3D (two connections, use PREPARE/EXECUTE)
--echo #    connection  action
--echo #    default:    $create_table
--echo #    con2:       $create_table (expect to get ER_TABLE_EXISTS_ERROR)
--echo #    default:    $drop_table
--echo #    con2:       $drop_table (expect to get ER_BAD_TABLE_ERROR)
--disable_query_log
--disable_result_log
connection default;
eval PREPARE create_table FROM "$create_table";
eval PREPARE drop_table FROM "$drop_table";
EXECUTE create_table;
connection con2;
eval PREPARE create_table FROM "$create_table";
eval PREPARE drop_table FROM "$drop_table";
EXECUTE drop_table;
connection default;
let $run= 1;
# Determine the current time.
EXECUTE stmt_start;
# Run execution loops till the planned runtime is reached
while ($run)
{
   let $loop_run= $loop_size;
   while ($loop_run)
   {
      EXECUTE create_table;
      connection con2;
      --error 0,ER_TABLE_EXISTS_ERROR
      EXECUTE create_table;
      if (!$mysql_errno)
      {
         --echo # Error: CREATE TABLE was successful though we expected ER_TABLE_EXISTS_ERROR
         --echo # abort
         exit;
      }
      connection default;
      EXECUTE drop_table;
      connection con2;
      --error 0,ER_BAD_TABLE_ERROR
      EXECUTE drop_table;
      if (!$mysql_errno)
      {
         --echo # Error: DROP TABLE was successful though we expected ER_BAD_TABLE_ERROR)
         --echo # abort
         exit;
      }
      connection default;
      dec $loop_run;
   }
   if (`EXECUTE stmt_break`)
   {
      let $run= 0;
   }
}
DEALLOCATE PREPARE create_table;
DEALLOCATE PREPARE drop_table;
connection con2;
DEALLOCATE PREPARE create_table;
DEALLOCATE PREPARE drop_table;
connection default;
--enable_result_log
--enable_query_log
