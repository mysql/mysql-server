################################################################################
# inc/partition_check_read2.inc                                                #
#                                                                              #
# Purpose:                                                                     #
#   Read rows from table t1 in different ways                                  #
#   This routine is only useful for the partition_<feature>_<engine> tests.    #
#                                                                              #
#------------------------------------------------------------------------------#
# Original Author: mleich                                                      #
# Original Date: 2006-04-11                                                    #
# Change Author:                                                               #
# Change Date:                                                                 #
# Change:                                                                      #
################################################################################

-- disable_query_log
-- disable_result_log
ANALYZE TABLE t1;
-- enable_result_log
-- enable_query_log

## EXPLAIN PARTITIONS SELECT for one single row
EXPLAIN PARTITIONS SELECT COUNT(*) <> 1 FROM t1 WHERE f_int1 = 3;

## Check SELECT for one single row
let $success= 1;
if ($no_debug)
{
--disable_query_log
}
SELECT COUNT(*) <> 1 INTO @aux FROM t1 WHERE f_int1 = 3;
--enable_query_log
let $run= `SELECT @aux`;
if ($run)
{
   --echo #      Unexpected result on SELECT
   eval SELECT COUNT(*) <> 1 FROM t1 WHERE f_int1 = 3;
   let $success= 0;
}
--echo # check read single success: $success

## Read all existing records in one step
let $success= 1;
if ($no_debug)
{
--disable_query_log
}
eval SELECT COUNT(*) <> @max_row INTO @aux FROM t1;
--enable_query_log
let $run= `SELECT @aux`;
if ($run)
{
   --echo #      Unexpected result on SELECT
   eval SELECT COUNT(*) <> @max_row FROM t1;
   let $success= 0;
}
--echo # check read all success: $success

## Read all existing records row by row
let $success= 1;
let $num= `SELECT @max_row`;
while ($num)
{
   if ($no_debug)
   {
   --disable_query_log
   }
   eval SELECT COUNT(*) <> 1 INTO @aux FROM t1
        WHERE f_int1 = 3;
   --enable_query_log
   let $run= `SELECT @aux`;
   if ($run)
   {
      --echo #      Unexpected result on SELECT
      eval SELECT COUNT(*) <> 1 FROM t1
           WHERE f_int1 = 3;
      let $success= 0;
   }
   dec $num;
}
--echo # check read row by row success: $success
