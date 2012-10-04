################################################################################
# inc/partition_check_read1.inc                                                #
#                                                                              #
# Purpose:                                                                     #
#   Read rows from table t1 in different ways                                  #
#   This routine is only useful for the partition_<feature>_<engine> tests.    #
#                                                                              #
# The parameter                                                                #
#   @exp_row_count -- expected number of rows within t1                        #
# must be set before sourcing this routine.                                    #
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
EXPLAIN PARTITIONS SELECT COUNT(*) FROM t1 WHERE f_date = '1000-02-10';

## Check SELECT for one single row
let $success= 1;
if ($no_debug)
{
--disable_query_log
}
SELECT COUNT(*) <> 1 INTO @aux FROM t1 WHERE f_date = '1000-02-10';
--enable_query_log
let $run= `SELECT @aux`;
if ($run)
{
   --echo #      Unexpected result on SELECT
   eval SELECT COUNT(*) <> 1 FROM t1 WHERE f_date = '1000-02-10';
   let $success= 0;
}
--echo # check read single success: $success

## Read all existing records in one step
let $success= 1;
if ($no_debug)
{
--disable_query_log
}
eval SELECT COUNT(*) <> @exp_row_count INTO @aux FROM t1;
--enable_query_log
let $run= `SELECT @aux`;
if ($run)
{
   --echo #      Unexpected result on SELECT
   eval SELECT COUNT(*) <> @exp_row_count FROM t1;
   let $success= 0;
}
--echo # check read all success: $success

## Read all existing records row by row
let $success= 1;
let $num= `SELECT @exp_row_count`;
while ($num)
{
   if ($no_debug)
   {
   --disable_query_log
   }
   eval SELECT COUNT(*) <> 1 INTO @aux FROM t1
        WHERE f_date = CONCAT(CAST(999 + $num AS CHAR),'-02-10');
   --enable_query_log
   let $run= `SELECT @aux`;
   if ($run)
   {
      --echo #      Unexpected result on SELECT
      eval SELECT COUNT(*) <> 1 FROM t1
           WHERE f_date = CONCAT(CAST(999 + $num AS CHAR),'-02-10');
      let $success= 0;
   }
   dec $num;
}
--echo # check read row by row success: $success
