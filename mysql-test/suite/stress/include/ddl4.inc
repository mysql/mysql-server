######## include/ddl4.inc ######
#
# Stress the storage engine with rapid CREATE/DROP TABLE/INDEX
# and following SELECT/INSERT/SHOW etc.
# Subtest 4 variants (4A - 4D)
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
# Settings for Subtest 4 variants
# Scenario: CREATE TABLE variant1/SHOW/DROP TABLE/SHOW(F)/
#           CREATE TABLE variant2/SHOW/DROP TABLE
let $create_table1=  CREATE TABLE t1 (f1 BIGINT, f2 BIGINT, UNIQUE(f1), UNIQUE (f2)) ENGINE=$engine_type;
let $create_table2=  CREATE TABLE t1 (f1 BIGINT) ENGINE=$engine_type;
let $show_table=     SHOW CREATE TABLE t1;
let $drop_table=     DROP TABLE t1;
#
eval $create_table1;
let $cr_value1= INITIALIZED;
let $cr_value1= query_get_value($show_table, Create Table, 1);
eval $drop_table;
eval $create_table2;
let $cr_value2= INITIALIZED;
let $cr_value2= query_get_value($show_table, Create Table, 1);
eval $drop_table;
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
--echo # Subtest 4A (one connection, no PREPARE/EXECUTE)
--echo #    connection  action
--echo #    default:    $create_table1
--echo #    default:    $show_table
--echo #    default:    $drop_table
--echo #    default:    $show_table (expect to get ER_NO_SUCH_TABLE)
--echo #    default:    $create_table2
--echo #    default:    $show_table
--echo #    default:    $drop_table
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
      eval $create_table1;
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
      eval $drop_table;
      --error 0,ER_NO_SUCH_TABLE
      eval $show_table;
      if (!$mysql_errno)
      {
         --echo # Error: SHOW CREATE TABLE was successful though we expected ER_NO_SUCH_TABLE)
         --echo # abort
         exit;
      }
      eval $create_table2;
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
      eval $drop_table;
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
--echo # Subtest 4B (one connection, use PREPARE/EXECUTE)
--echo #    connection  action
--echo #    default:    $create_table1
--echo #    default:    $show_table
--echo #    default:    $drop_table
--echo #    default:    $show_table (expect to get ER_NO_SUCH_TABLE)
--echo #    default:    $create_table2
--echo #    default:    $show_table
--echo #    default:    $drop_table
--disable_query_log
--disable_result_log
connection default;
eval PREPARE create_table1 FROM "$create_table1";
eval PREPARE create_table2 FROM "$create_table2";
EXECUTE create_table1;
eval PREPARE show_table FROM "$show_table";
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
      EXECUTE create_table1;
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
      EXECUTE drop_table;
      --error 0,ER_NO_SUCH_TABLE
      EXECUTE show_table;
      if (!$mysql_errno)
      {
         --echo # Error: SHOW CREATE TABLE was successful though we expected ER_NO_SUCH_TABLE)
         --echo # abort
         exit;
      }
      EXECUTE create_table2;
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
      EXECUTE drop_table;
      dec $loop_run;
   }
   if (`EXECUTE stmt_break`)
   {
      let $run= 0;
   }
}
DEALLOCATE PREPARE create_table1;
DEALLOCATE PREPARE create_table2;
DEALLOCATE PREPARE show_table;
DEALLOCATE PREPARE drop_table;
--enable_result_log
--enable_query_log
#
--echo # Subtest 4C (two connections, no PREPARE/EXECUTE)
--echo #    connection  action
--echo #    default:    $create_table1
--echo #    con2:       $show_table
--echo #    default:    $drop_table
--echo #    con2:       $show_table (expect to get ER_NO_SUCH_TABLE)
--echo #    default:    $create_table2
--echo #    con2:       $show_table
--echo #    default:    $drop_table
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
      eval $create_table1;
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
      eval $drop_table;
      connection con2;
      --error 0,ER_NO_SUCH_TABLE
      eval $show_table;
      if (!$mysql_errno)
      {
         --echo # Error: SHOW CREATE TABLE was successful though we expected ER_NO_SUCH_TABLE)
         --echo # abort
         exit;
      }
      connection default;
      eval $create_table2;
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
      eval $drop_table;
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
--echo # Subtest 4D (two connections, use PREPARE/EXECUTE)
--echo #    connection  action
--echo #    default:    $create_table1
--echo #    con2:       $show_table
--echo #    default:    $drop_table
--echo #    con2:       $show_table (expect to get ER_NO_SUCH_TABLE)
--echo #    default:    $create_table2
--echo #    con2:       $show_table
--echo #    default:    $drop_table
--disable_query_log
--disable_result_log
connection default;
eval PREPARE create_table1 FROM "$create_table1";
eval PREPARE create_table2 FROM "$create_table2";
eval PREPARE drop_table FROM "$drop_table";
EXECUTE create_table1;
connection con2;
eval PREPARE show_table FROM "$show_table";
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
      EXECUTE create_table1;
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
      EXECUTE drop_table;
      connection con2;
      --error 0,ER_NO_SUCH_TABLE
      EXECUTE show_table;
      if (!$mysql_errno)
      {
         --echo # Error: SHOW CREATE TABLE was successful though we expected ER_NO_SUCH_TABLE)
         --echo # abort
         exit;
      }
      connection default;
      EXECUTE create_table2;
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
      EXECUTE drop_table;
      dec $loop_run;
   }
   if (`EXECUTE stmt_break`)
   {
      let $run= 0;
   }
}
DEALLOCATE PREPARE create_table1;
DEALLOCATE PREPARE create_table2;
DEALLOCATE PREPARE drop_table;
connection con2;
DEALLOCATE PREPARE show_table;
connection default;
--enable_result_log
--enable_query_log
