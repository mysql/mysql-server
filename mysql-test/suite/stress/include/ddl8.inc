######## include/ddl8.inc ######
#
# Stress the storage engine with rapid CREATE/DROP TABLE/INDEX
# and following SELECT/INSERT/SHOW etc.
# Subtest 8 variants (8A - 8D)
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
# Settings for Subtest 8 variants
# Scenario: CREATE INDEX variant1/SHOW/DROP INDEX/
#           CREATE INDEX variant2/SHOW/DROP INDEX
let $create_index1=  CREATE INDEX IDX ON t1 (f2);
let $create_index2=  CREATE UNIQUE INDEX IDX ON t1 (f2);
let $show_table=     SHOW CREATE TABLE t1;
let $drop_index=     DROP INDEX IDX ON t1;
eval CREATE TABLE t1 (f1 BIGINT, f2 BIGINT, UNIQUE(f1)) ENGINE=$engine_type;
INSERT INTO t1 VALUES(1,1);
eval $create_index1;
let $cr_value1= INITIALIZED;
let $cr_value1= query_get_value($show_table, Create Table, 1);
eval $drop_index;
eval $create_index2;
let $cr_value2= INITIALIZED;
let $cr_value2= query_get_value($show_table, Create Table, 1);
eval $drop_index;
if (`SELECT '$cr_value1' = '$cr_value2'`)
{
   --echo # Error during generation of prerequisites.
   --echo # cr_value1 equals cr_value2
   --echo # cr_value1: $cr_value1
   --echo # cr_value2: $cr_value2
   --echo # abort
   exit;
}
#----------------------------------------------------------------------

#
--echo # Subtest 8A (one connection, no PREPARE/EXECUTE)
--echo #    connection  action
--echo #    default:    $create_index1
--echo #    default:    $show_table
--echo #    default:    $drop_index
--echo #    default:    $create_index2
--echo #    default:    $show_table
--echo #    default:    $drop_index
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
      eval $create_index1;
      let $value= INITIALIZED;
      let $value= query_get_value($show_table, Create Table, 1);
      if (`SELECT '$value' <> '$cr_value1'`)
      {
         --echo # Error: Unexpected SHOW CREATE TABLE output
         --echo # Got:      $value
         --echo # Expected: $cr_value1
         --echo # abort
         exit;
      }
      eval $drop_index;
      eval $create_index2;
      let $value= INITIALIZED;
      let $value= query_get_value($show_table, Create Table, 1);
      if (`SELECT '$value' <> '$cr_value2'`)
      {
         --echo # Error: Unexpected SHOW CREATE TABLE output
         --echo # Got:      $value
         --echo # Expected: $cr_value2
         --echo # abort
         exit;
      }
      eval $drop_index;
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
--echo # Subtest 8B (one connection, use PREPARE/EXECUTE)
--echo #    connection  action
--echo #    default:    $create_index1
--echo #    default:    $show_table
--echo #    default:    $drop_index
--echo #    default:    $create_index2
--echo #    default:    $show_table
--echo #    default:    $drop_index
--disable_query_log
--disable_result_log
connection default;
eval PREPARE create_index1 FROM "$create_index1";
eval PREPARE create_index2 FROM "$create_index2";
EXECUTE create_index1;
eval PREPARE show_table FROM "$show_table";
eval PREPARE drop_index FROM "$drop_index";
EXECUTE drop_index;
let $run= 1;
# Determine the current time.
EXECUTE stmt_start;
# Run execution loops till the planned runtime is reached
while ($run)
{
   let $loop_run= $loop_size;
   while ($loop_run)
   {
      EXECUTE create_index1;
      let $value= INITIALIZED;
      let $value= query_get_value(EXECUTE show_table, Create Table, 1);
      if (`SELECT '$value' <> '$cr_value1'`)
      {
         --echo # Error: Unexpected SHOW CREATE TABLE output
         --echo # Got:      $value
         --echo # Expected: $cr_value1
         --echo # abort
         exit;
      }
      EXECUTE drop_index;
      EXECUTE create_index2;
      let $value= INITIALIZED;
      let $value= query_get_value(EXECUTE show_table, Create Table, 1);
      if (`SELECT '$value' <> '$cr_value2'`)
      {
         --echo # Error: Unexpected SHOW CREATE TABLE output
         --echo # Got:      $value
         --echo # Expected: $cr_value2
         --echo # abort
         exit;
      }
      EXECUTE drop_index;
      dec $loop_run;
   }
   if (`EXECUTE stmt_break`)
   {
      let $run= 0;
   }
}
DEALLOCATE PREPARE create_index1;
DEALLOCATE PREPARE create_index2;
DEALLOCATE PREPARE show_table;
DEALLOCATE PREPARE drop_index;
--enable_result_log
--enable_query_log
#
--echo # Subtest 8C (two connections, no PREPARE/EXECUTE)
--echo #    connection  action
--echo #    default:    $create_index1
--echo #    con2:       $show_table
--echo #    default:    $drop_index
--echo #    default:    $create_index2
--echo #    con2:       $show_table
--echo #    default:    $drop_index
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
      eval $create_index1;
      connection con2;
      let $value= INITIALIZED;
      let $value= query_get_value($show_table, Create Table, 1);
      if (`SELECT '$value' <> '$cr_value1'`)
      {
         --echo # Error: Unexpected SHOW CREATE TABLE output
         --echo # Got:      $value
         --echo # Expected: $cr_value1
         --echo # abort
         exit;
      }
      connection default;
      eval $drop_index;
      eval $create_index2;
      connection con2;
      let $value= INITIALIZED;
      let $value= query_get_value($show_table, Create Table, 1);
      if (`SELECT '$value' <> '$cr_value2'`)
      {
         --echo # Error: Unexpected SHOW CREATE TABLE output
         --echo # Got:      $value
         --echo # Expected: $cr_value2
         --echo # abort
         exit;
      }
      connection default;
      eval $drop_index;
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
--echo # Subtest 8D (two connections, use PREPARE/EXECUTE)
--echo #    connection  action
--echo #    default:    $create_index1
--echo #    con2:       $show_table
--echo #    default:    $drop_index
--echo #    default:    $create_index2
--echo #    con2:       $show_table
--echo #    default:    $drop_index
--disable_query_log
--disable_result_log
connection default;
eval PREPARE create_index1 FROM "$create_index1";
eval PREPARE create_index2 FROM "$create_index2";
eval PREPARE drop_index FROM "$drop_index";
EXECUTE create_index1;
connection con2;
eval PREPARE show_table FROM "$show_table";
connection default;
EXECUTE drop_index;
let $run= 1;
# Determine the current time.
EXECUTE stmt_start;
# Run execution loops till the planned runtime is reached
while ($run)
{
   let $loop_run= $loop_size;
   while ($loop_run)
   {
      EXECUTE create_index1;
      connection con2;
      let $value= INITIALIZED;
      let $value= query_get_value(EXECUTE show_table, Create Table, 1);
      if (`SELECT '$value' <> '$cr_value1'`)
      {
         --echo # Error: Unexpected SHOW CREATE TABLE output
         --echo # Got:      $value
         --echo # Expected: $cr_value1
         --echo # abort
         exit;
      }
      connection default;
      EXECUTE drop_index;
      EXECUTE create_index2;
      connection con2;
      let $value= INITIALIZED;
      let $value= query_get_value(EXECUTE show_table, Create Table, 1);
      if (`SELECT '$value' <> '$cr_value2'`)
      {
         --echo # Error: Unexpected SHOW CREATE TABLE output
         --echo # Got:      $value
         --echo # Expected: $cr_value2
         --echo # abort
         exit;
      }
      connection default;
      EXECUTE drop_index;
      dec $loop_run;
   }
   if (`EXECUTE stmt_break`)
   {
      let $run= 0;
   }
}
DEALLOCATE PREPARE create_index1;
DEALLOCATE PREPARE create_index2;
DEALLOCATE PREPARE drop_index;
connection con2;
DEALLOCATE PREPARE show_table;
connection default;
--enable_result_log
--enable_query_log

DROP TABLE t1;
