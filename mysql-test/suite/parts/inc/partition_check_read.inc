################################################################################
# inc/partition_check_read.inc                                                 #
#                                                                              #
# Purpose:                                                                     #
#   Read table t1 row by row                                                   #
#   This routine is only useful for the partition_<feature>_<engine> tests.    #
#                                                                              #
# Note: If this routine is successful, at least the following is fullfilled    #
#       - select single row via $col_to_check basically works -- no crash      #
#       - the table contains all expected rows                                 #
#                                                                              #
#------------------------------------------------------------------------------#
# Original Author: mleich                                                      #
# Original Date: 2006-03-05                                                    #
# Change Author:                                                               #
# Change Date:                                                                 #
# Change:                                                                      #
################################################################################

let $success= 1;

## Read all existing records
let $num= `SELECT @max_row`;
while ($num)
{
   if ($no_debug)
   {
   --disable_query_log
   }
   eval SELECT COUNT(*) <> 1 INTO @aux FROM t1 WHERE $col_to_check = $num;
   --enable_query_log
   let $run= `SELECT @aux`;
   if ($run)
   {
      --echo #      Unexpected result on SELECT
      eval SELECT COUNT(*) <> 1 FROM t1 WHERE $col_to_check = $num;
      let $success= 0;
   }
   dec $num;
}
## Read some not existing records
let $num= `SELECT @max_row_div2`;
while ($num)
{
   if ($no_debug)
   {
   --disable_query_log
   }
   eval SELECT COUNT(*) = 1 INTO @aux FROM t1 WHERE $col_to_check = @max_row + $num;
   --enable_query_log
   let $run= `SELECT @aux`;
   if ($run)
   {
      --echo #      Unexpected result on SELECT
      eval SELECT COUNT(*) <> 1 FROM t1 WHERE $col_to_check = @max_row + $num;
      let $success= 0;
   }
   dec $num;
}
let $num= `SELECT @max_row_div2`;
while ($num)
{
   if ($no_debug)
   {
   --disable_query_log
   }
   eval SELECT COUNT(*) = 1 INTO @aux FROM t1 WHERE $col_to_check = 1 - $num;
   --enable_query_log
   let $run= `SELECT @aux`;
   if ($run)
   {
      --echo #      Unexpected result on SELECT
      eval SELECT COUNT(*) <> 1 FROM t1 WHERE $col_to_check = 1 - $num;
      let $success= 0;
   }
   dec $num;
}
if ($no_debug)
{
--disable_query_log
}
--echo # check read via $col_to_check success: $success
# mleich: The following is omitted because of not reported mysqltest bug
#     (@max_row time the success message)
if (0)
{
eval SELECT '# check read via $col_to_check success: ' AS "", $success AS "" FROM t1;
}
--enable_query_log
