######## include/ddl7.inc ######
#
# Stress the storage engine with rapid CREATE/DROP TABLE/INDEX
# and following SELECT/INSERT/SHOW etc.
# Subtest 7 variants (7A - 7D)
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
# Settings for Subtest 7 variants
# Scenario: CREATE INDEX/INSERT(F)/DROP INDEX/INSERT/CREATE INDEX(F)/DELETE
let $create_index=   CREATE UNIQUE INDEX IDX1 ON t1 (f2);
let $insert_record=  INSERT INTO t1 VALUES(1,1);
let $drop_index=     DROP INDEX IDX1 ON t1;
let $delete_record=  DELETE FROM t1 WHERE f1 = 1;
eval CREATE TABLE t1 (f1 BIGINT, f2 BIGINT, PRIMARY KEY(f1)) ENGINE=$engine_type;
INSERT INTO t1 VALUES(0,1);
#----------------------------------------------------------------------

#
--echo # Subtest 7A (one connection, no PREPARE/EXECUTE)
--echo #    connection  action
--echo #    default:    $create_index
--echo #    default:    $insert_record (expect to get ER_DUP_ENTRY)
--echo #    default:    $drop_index
--echo #    default:    $insert_record
--echo #    default:    $create_index  (expect to get ER_DUP_ENTRY)
--echo #    default:    $delete_record
--disable_query_log
--disable_result_log
connection default;
let $run= 1;
let $counter= 1;
# Determine the current time.
EXECUTE stmt_start;
# Run execution loops till the planned runtime is reached
while ($run)
{
   let $loop_run= $loop_size;
   while ($loop_run)
   {
      eval $create_index;
      --error 0,ER_DUP_ENTRY
      eval $insert_record;
      if (!$mysql_errno)
      {
         --echo # Error: INSERT was successful though we expected ER_DUP_ENTRY
         --echo # abort
         exit;
      }
      eval $drop_index;
      eval $insert_record;
      # NDB: ER_DUP_UNIQUE, others: ER_DUP_ENTRY
      --error 0,ER_DUP_ENTRY,ER_DUP_UNIQUE
      eval $create_index;
      if (!$mysql_errno)
      {
         --echo # Error: CREATE INDEX was successful though we expected ER_DUP_ENTRY
         --echo # Error: or ER_DUP_UNIQUE (NDB only)
         --echo # abort
         exit;
      }
      eval $delete_record;
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
--echo # Subtest 7B (one connection, use PREPARE/EXECUTE)
--echo #    connection  action
--echo #    default:    $create_index
--echo #    default:    $insert_record (expect to get ER_DUP_ENTRY)
--echo #    default:    $drop_index
--echo #    default:    $insert_record
--echo #    default:    $create_index  (expect to get ER_DUP_ENTRY)
--echo #    default:    $delete_record
--disable_query_log
--disable_result_log
connection default;
eval PREPARE create_index FROM "$create_index";
eval PREPARE insert_record FROM "$insert_record";
eval PREPARE delete_record FROM "$delete_record";
eval PREPARE drop_index FROM "$drop_index";
let $run= 1;
let $counter= 1;
# Determine the current time.
EXECUTE stmt_start;
# Run execution loops till the planned runtime is reached
while ($run)
{
   let $loop_run= $loop_size;
   while ($loop_run)
   {
      EXECUTE create_index;
      --error 0,ER_DUP_ENTRY
      EXECUTE insert_record;
      if (!$mysql_errno)
      {
         --echo # Error: INSERT was successful though we expected ER_DUP_ENTRY
         --echo # abort
         exit;
      }
      EXECUTE drop_index;
      EXECUTE insert_record;
      --error 0,ER_DUP_ENTRY
      EXECUTE create_index;
      if (!$mysql_errno)
      {
         --echo # Error: CREATE INDEX was successful though we expected ER_DUP_ENTRY
         --echo # abort
         exit;
      }
      EXECUTE delete_record;
      dec $loop_run;
   }
   if (`EXECUTE stmt_break`)
   {
      let $run= 0;
   }
}
DEALLOCATE PREPARE create_index;
DEALLOCATE PREPARE insert_record;
DEALLOCATE PREPARE delete_record;
DEALLOCATE PREPARE drop_index;
--enable_result_log
--enable_query_log
#
--echo # Subtest 7C (two connections, no PREPARE/EXECUTE)
--echo #    connection  action
--echo #    default:    $create_index
--echo #    default:    $insert_record (expect to get ER_DUP_ENTRY)
--echo #    con2:       $drop_index
--echo #    default:    $insert_record
--echo #    con2:       $create_index  (expect to get ER_DUP_ENTRY)
--echo #    con2:       $delete_record
--disable_query_log
--disable_result_log
connection default;
let $run= 1;
let $counter= 1;
# Determine the current time.
EXECUTE stmt_start;
# Run execution loops till the planned runtime is reached
while ($run)
{
   let $loop_run= $loop_size;
   while ($loop_run)
   {
      eval $create_index;
      --error 0,ER_DUP_ENTRY
      eval $insert_record;
      if (!$mysql_errno)
      {
         --echo # Error: INSERT was successful though we expected ER_DUP_ENTRY
         --echo # abort
         exit;
      }
      connection con2;
      eval $drop_index;
      connection default;
      eval $insert_record;
      connection con2;
      --error 0,ER_DUP_ENTRY
      eval $create_index;
      if (!$mysql_errno)
      {
         --echo # Error: CREATE INDEX was successful though we expected ER_DUP_ENTRY
         --echo # abort
         exit;
      }
      eval $delete_record;
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
--echo # Subtest 7D (two connections, use PREPARE/EXECUTE)
--echo #    connection  action
--echo #    default:    $create_index
--echo #    default:    $insert_record (expect to get ER_DUP_ENTRY)
--echo #    con2:       $drop_index
--echo #    default:    $insert_record
--echo #    con2:       $create_index  (expect to get ER_DUP_ENTRY)
--echo #    con2:       $delete_record
--disable_query_log
--disable_result_log
connection default;
eval PREPARE create_index FROM "$create_index";
eval PREPARE insert_record FROM "$insert_record";
EXECUTE create_index;
connection con2;
eval PREPARE create_index FROM "$create_index";
eval PREPARE drop_index FROM "$drop_index";
eval PREPARE delete_record FROM "$delete_record";
EXECUTE drop_index;
connection default;
let $run= 1;
let $counter= 1;
# Determine the current time.
EXECUTE stmt_start;
# Run execution loops till the planned runtime is reached
while ($run)
{
   let $loop_run= $loop_size;
   while ($loop_run)
   {
      EXECUTE create_index;
      --error 0,ER_DUP_ENTRY
      EXECUTE insert_record;
      if (!$mysql_errno)
      {
         --echo # Error: INSERT was successful though we expected ER_DUP_ENTRY
         --echo # abort
         exit;
      }
      connection con2;
      EXECUTE drop_index;
      connection default;
      EXECUTE insert_record;
      connection con2;
      --error 0,ER_DUP_ENTRY
      EXECUTE create_index;
      if (!$mysql_errno)
      {
         --echo # Error: CREATE INDEX was successful though we expected ER_DUP_ENTRY
         --echo # abort
         exit;
      }
      EXECUTE delete_record;
      connection default;
      dec $loop_run;
   }
   if (`EXECUTE stmt_break`)
   {
      let $run= 0;
   }
}
DEALLOCATE PREPARE create_index;
DEALLOCATE PREPARE insert_record;
connection con2;
DEALLOCATE PREPARE create_index;
DEALLOCATE PREPARE drop_index;
DEALLOCATE PREPARE delete_record;
connection default;
--enable_result_log
--enable_query_log

DROP TABLE t1;
