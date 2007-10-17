######## include/ddl5.inc ######
#
# Stress the storage engine with rapid CREATE/DROP TABLE/INDEX
# and following SELECT/INSERT/SHOW etc.
# Subtest 5 variants (5A - 5D)
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
# Settings for Subtest 5 variants
# Scenario: CREATE TABLE with AUTOINC/INSERT/SELECT/DROP TABLE
let $create_table=   CREATE TABLE t1 (f1 MEDIUMINT NOT NULL AUTO_INCREMENT,f2 BIGINT, UNIQUE(f1)) ENGINE = $engine_type;
let $insert_into=    INSERT INTO t1 SET f2 = 9;
let $select_record=  SELECT f1 <> 1 OR f1 IS NULL FROM t1 WHERE f2 = 9;
let $drop_table=     DROP TABLE t1;
#----------------------------------------------------------------------

#
--echo # Subtest 5A (one connection, no PREPARE/EXECUTE)
--echo #    connection  action
--echo #    default:    $create_table
--echo #    default:    $insert_into
--echo #    default:    $select_record
--echo #    default:    $drop_table
--disable_query_log
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
      eval $insert_into;
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
      dec $loop_run;
   }
   if (`EXECUTE stmt_break`)
   {
      let $run= 0;
   }
}
--enable_query_log
#
--echo # Subtest 5B (one connection, use PREPARE/EXECUTE)
--echo #    connection  action
--echo #    default:    $create_table
--echo #    default:    $insert_into
--echo #    default:    $select_record
--echo #    default:    $drop_table
--disable_query_log
connection default;
eval PREPARE create_table FROM "$create_table";
EXECUTE create_table;
eval PREPARE insert_into FROM "$insert_into";
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
      EXECUTE insert_into;
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
      dec $loop_run;
   }
   if (`EXECUTE stmt_break`)
   {
      let $run= 0;
   }
}
DEALLOCATE PREPARE create_table;
DEALLOCATE PREPARE insert_into;
DEALLOCATE PREPARE select_record;
DEALLOCATE PREPARE drop_table;
--enable_query_log
#
--echo # Subtest 5C (two connections, no PREPARE/EXECUTE)
--echo #    connection  action
--echo #    default:    $create_table
--echo #    con2:       $insert_into
--echo #    default:    $select_record
--echo #    con2:       $drop_table
--disable_query_log
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
      eval $insert_into;
      connection default;
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
      connection con2;
      eval $drop_table;
      connection default;
      dec $loop_run;
   }
   if (`EXECUTE stmt_break`)
   {
      let $run= 0;
   }
}
--enable_query_log
#
--echo # Subtest 5D (two connections, use PREPARE/EXECUTE)
--echo #    connection  action
--echo #    default:    $create_table
--echo #    con2:       $insert_into
--echo #    default:    $select_record
--echo #    con2:       $drop_table
--disable_query_log
connection default;
eval PREPARE create_table FROM "$create_table";
EXECUTE create_table;
eval PREPARE select_record FROM "$select_record";
connection con2;
eval PREPARE insert_into FROM "$insert_into";
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
      EXECUTE insert_into;
      connection default;
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
      connection con2;
      EXECUTE drop_table;
      connection default;
      dec $loop_run;
   }
   if (`EXECUTE stmt_break`)
   {
      let $run= 0;
   }
}
DEALLOCATE PREPARE create_table;
DEALLOCATE PREPARE select_record;
connection con2;
DEALLOCATE PREPARE insert_into;
DEALLOCATE PREPARE drop_table;
connection default;
--enable_query_log
