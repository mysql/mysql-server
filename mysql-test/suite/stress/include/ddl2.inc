######## include/ddl2.inc ######
#
# Stress the storage engine with rapid CREATE/DROP TABLE/INDEX
# and following SELECT/INSERT/SHOW etc.
# Subtest 2 variants (2A - 2D)
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
# Please look for more details within include/ddl1.inc.
#
# Creation of this test:
# 2007-07-04 mleich
#


#----------------------------------------------------------------------
# Settings for Subtest 2 variants
# Scenario: CREATE TABLE AS SELECT/SELECT/DROP/SELECT(F)
let $create_table=  CREATE TABLE t1 ENGINE = $engine_type AS SELECT 1 AS f1;
let $select_record= SELECT COUNT(*) <> 1 FROM t1 WHERE f1 = 1;
let $drop_table=    DROP TABLE t1;
#----------------------------------------------------------------------

#
--echo # Subtest 2A (one connection, no PREPARE/EXECUTE)
--echo #    connection  action
--echo #    default:    $create_table
--echo #    default:    $select_record
--echo #    default:    $drop_table
--echo #    default:    $select_record (expect to get ER_NO_SUCH_TABLE)
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
      if (`$select_record`)
      {
         --enable_result_log
         --enable_query_log
         --echo # Error: Unexpected content within t1.
         --echo # Expected: 0
         --echo # Got:
         eval $select_record;
         SELECT * FROM t1;
         --echo # abort
         exit;
      }
      eval $drop_table;
      --error 0,ER_NO_SUCH_TABLE
      eval $select_record;
      if (!$mysql_errno)
      {
         --echo # Error: SELECT was successful though we expected ER_NO_SUCH_TABLE
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
--echo # Subtest 2B (one connection, use PREPARE/EXECUTE)
--echo #    connection  action
--echo #    default:    $create_table
--echo #    default:    $select_record
--echo #    default:    $drop_table
--echo #    default:    $select_record (expect to get ER_NO_SUCH_TABLE)
--disable_query_log
--disable_result_log
connection default;
eval PREPARE create_table FROM "$create_table";
EXECUTE create_table;
eval PREPARE select_record FROM "$select_record";
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
      if (`EXECUTE select_record`)
      {
         --enable_result_log
         --enable_query_log
         --echo # Error: Unexpected content within t1.
         --echo # Expected: 0
         --echo # Got:
         EXECUTE select_record;
         SELECT * FROM t1;
         --echo # abort
         exit;
      }
      EXECUTE drop_table;
      --error 0,ER_NO_SUCH_TABLE
      EXECUTE select_record;
      if (!$mysql_errno)
      {
         --echo # Error: SELECT was successful though we expected ER_NO_SUCH_TABLE
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
DEALLOCATE PREPARE select_record;
DEALLOCATE PREPARE drop_table;
--enable_result_log
--enable_query_log
#
--echo # Subtest 2C (two connections, no PREPARE/EXECUTE)
--echo #    connection  action
--echo #    default:    $create_table
--echo #    con2:       $select_record
--echo #    default:    $drop_table
--echo #    con2:       $select_record (expect to get ER_NO_SUCH_TABLE)
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
      if (`$select_record`)
      {
         --enable_result_log
         --enable_query_log
         --echo # Error: Unexpected content within t1.
         --echo # Expected: 0
         --echo # Got:
         eval $select_record;
         SELECT * FROM t1;
         --echo # abort
         exit;
      }
      connection default;
      eval $drop_table;
      connection con2;
      --error 0,ER_NO_SUCH_TABLE
      eval $select_record;
      if (!$mysql_errno)
      {
         --echo # Error: SELECT was successful though we expected ER_NO_SUCH_TABLE
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
--echo # Subtest 2D (two connections, use PREPARE/EXECUTE)
--echo #    connection  action
--echo #    default:    $create_table
--echo #    con2:       $select_record
--echo #    default:    $drop_table
--echo #    con2:       $select_record (expect to get ER_NO_SUCH_TABLE)
--disable_query_log
--disable_result_log
connection default;
eval PREPARE create_table FROM "$create_table";
eval PREPARE drop_table FROM "$drop_table";
EXECUTE create_table;
connection con2;
eval PREPARE select_record FROM "$select_record";
connection default;
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
      connection con2;
      if (`EXECUTE select_record`)
      {
         --enable_result_log
         --enable_query_log
         --echo # Error: Unexpected content within t1.
         --echo # Expected: 0
         --echo # Got:
         EXECUTE select_record;
         SELECT * FROM t1;
         --echo # abort
         exit;
      }
      connection default;
      EXECUTE drop_table;
      connection con2;
      --error 0,ER_NO_SUCH_TABLE
      EXECUTE select_record;
      if (!$mysql_errno)
      {
         --echo # Error: SELECT was successful though we expected ER_NO_SUCH_TABLE
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
DEALLOCATE PREPARE select_record;
connection default;
--enable_result_log
--enable_query_log
