######## include/ddl6.inc ######
#
# Stress the storage engine with rapid CREATE/DROP TABLE/INDEX
# and following SELECT/INSERT/SHOW etc.
# Subtest 6 variants (6A - 6D)
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
# Settings for Subtest 6 variants
# Scenario: CREATE INDEX/CREATE INDEX(F)/DROP INDEX/DROP INDEX(F)
let $create_index=  CREATE INDEX IDX1 ON t1 (f2);
let $drop_index=    DROP INDEX IDX1 ON t1;
eval CREATE TABLE t1 (f1 BIGINT, f2 BIGINT, UNIQUE(f1)) ENGINE=$engine_type;
#----------------------------------------------------------------------


#
--echo # Subtest 6A (one connection, no PREPARE/EXECUTE)
--echo #    connection  action
--echo #    default:    $create_index
--echo #    default:    $create_index (expect to get ER_DUP_KEYNAME)
--echo #    default:    $drop_index
--echo #    default:    $drop_index (expect to get ER_CANT_DROP_FIELD_OR_KEY)
--echo #    default:    $create_index
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
      eval $create_index;
      --error 0,ER_DUP_KEYNAME
      eval $create_index;
      if (!$mysql_errno)
      {
         --echo # Error: CREATE INDEX was successful though we expected ER_DUP_KEYNAME
         --echo # abort
         exit;
      }
      eval $drop_index;
      --error 0,ER_CANT_DROP_FIELD_OR_KEY
      eval $drop_index;
      if (!$mysql_errno)
      {
         --echo # Error: DROP INDEX was successful though we expected ER_CANT_DROP_FIELD_OR_KEY
         --echo # abort
         exit;
      }
      eval $create_index;
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
--echo # Subtest 6B (one connection, use PREPARE/EXECUTE)
--echo #    connection  action
--echo #    default:    $create_index
--echo #    default:    $create_index (expect to get ER_DUP_KEYNAME)
--echo #    default:    $drop_index
--echo #    default:    $drop_index (expect to get ER_CANT_DROP_FIELD_OR_KEY)
--echo #    default:    $create_index
--echo #    default:    $drop_index
--disable_query_log
--disable_result_log
connection default;
eval PREPARE create_index FROM "$create_index";
EXECUTE create_index;
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
      EXECUTE create_index;
      --error 0,ER_DUP_KEYNAME
      EXECUTE create_index;
      if (!$mysql_errno)
      {
         --echo # Error: CREATE INDEX was successful though we expected ER_DUP_KEYNAME
         --echo # abort
         exit;
      }
      EXECUTE drop_index;
      --error 0,ER_CANT_DROP_FIELD_OR_KEY
      EXECUTE drop_index;
      if (!$mysql_errno)
      {
         --echo # Error: DROP INDEX was successful though we expected ER_CANT_DROP_FIELD_OR_KEY
         --echo # abort
         exit;
      }
      EXECUTE create_index;
      EXECUTE drop_index;
      dec $loop_run;
   }
   if (`EXECUTE stmt_break`)
   {
      let $run= 0;
   }
}
DEALLOCATE PREPARE create_index;
DEALLOCATE PREPARE drop_index;
--enable_result_log
--enable_query_log
#
--echo # Subtest 6C (two connections, no PREPARE/EXECUTE)
--echo #    connection  action
--echo #    default:    $create_index
--echo #    con2:       $create_index (expect to get ER_DUP_KEYNAME)
--echo #    default:    $drop_index
--echo #    con2:       $drop_index (expect to get ER_CANT_DROP_FIELD_OR_KEY)
--echo #    default:    $create_index
--echo #    con2:       $drop_index
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
      eval $create_index;
      connection con2;
      --error 0,ER_DUP_KEYNAME
      eval $create_index;
      if (!$mysql_errno)
      {
         --echo # Error: CREATE INDEX was successful though we expected ER_DUP_KEYNAME
         --echo # abort
         exit;
      }
      connection default;
      eval $drop_index;
      connection con2;
      --error 0,ER_CANT_DROP_FIELD_OR_KEY
      eval $drop_index;
      if (!$mysql_errno)
      {
         --echo # Error: DROP INDEX was successful though we expected ER_CANT_DROP_FIELD_OR_KEY
         --echo # abort
         exit;
      }
      connection default;
      eval $create_index;
      connection con2;
      eval $drop_index;
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
--echo # Subtest 6D (two connections, use PREPARE/EXECUTE)
--echo #    connection  action
--echo #    default:    $create_index
--echo #    con2:       $create_index (expect to get ER_DUP_KEYNAME)
--echo #    default:    $drop_index
--echo #    con2:       $drop_index (expect to get ER_CANT_DROP_FIELD_OR_KEY)
--echo #    default:    $create_index
--echo #    con2:       $drop_index
--disable_query_log
--disable_result_log
connection default;
eval PREPARE create_index FROM "$create_index";
eval PREPARE drop_index FROM "$drop_index";
EXECUTE create_index;
connection con2;
eval PREPARE create_index FROM "$create_index";
eval PREPARE drop_index FROM "$drop_index";
EXECUTE drop_index;
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
      EXECUTE create_index;
      connection con2;
      --error 0,ER_DUP_KEYNAME
      EXECUTE create_index;
      if (!$mysql_errno)
      {
         --echo # Error: CREATE INDEX was successful though we expected ER_DUP_KEYNAME
         --echo # abort
         exit;
      }
      connection default;
      EXECUTE drop_index;
      connection con2;
      --error 0,ER_CANT_DROP_FIELD_OR_KEY
      EXECUTE drop_index;
      if (!$mysql_errno)
      {
         --echo # Error: DROP INDEX was successful though we expected ER_CANT_DROP_FIELD_OR_KEY
         --echo # abort
         exit;
      }
      connection default;
      EXECUTE create_index;
      connection con2;
      EXECUTE drop_index;
      connection default;
      dec $loop_run;
   }
   if (`EXECUTE stmt_break`)
   {
      let $run= 0;
   }
}
DEALLOCATE PREPARE create_index;
DEALLOCATE PREPARE drop_index;
connection con2;
DEALLOCATE PREPARE create_index;
DEALLOCATE PREPARE drop_index;
connection default;
--enable_result_log
--enable_query_log

DROP TABLE t1;
