################################################################################
# include/partition_syntax_1.inc                                               #
#                                                                              #
# Purpose:                                                                     #
#   Auxiliary script, only useful when sourced by include/partition_syntax.inc #
#                                                                              #
#    Try to create a table with number of partitions/subpartitions             #
#    = $part_number. Print the layout of the table and drop it.                #
#                                                                              #
#    The parameter $part_number must be set before sourcing this script.       #
#                                                                              #
#------------------------------------------------------------------------------#
# Original Author: ML                                                          #
# Original Date: 2006-03-05                                                    #
# Change Author:                                                               #
# Change Date:                                                                 #
# Change:                                                                      #
################################################################################

--disable_abort_on_error
eval CREATE TABLE t1 (
$column_list
)
PARTITION BY HASH(f_int1) PARTITIONS $part_number;
--enable_abort_on_error
--disable_query_log
eval SET @my_errno= $mysql_errno ;
let $run= `SELECT @my_errno = 0`;
# Expected error codes are
# 0
# 1064 ERROR 42000: You have an error in your SQL syntax
#                   Reason: assign -1 partitions
# 1486 ERROR HY000: Too many partitions (including subpartitions) were defined
# 1491 ERROR HY000: Number of partitions = 0 is not an allowed value
let $unexpected_error= `SELECT @my_errno NOT IN (0,1064,1487,1492)`;
if ($unexpected_error)
{
   --echo #      The last command got an unexepected error response.
   --echo #      Expected/handled SQL codes are 0,1064,1487,1492
   SELECT '#      SQL code we got was: ' AS "", @my_errno AS "";
   --echo #      Sorry, have to abort.
   exit;
   --echo
}
--enable_query_log
#
# If this operation was successfull, print layout + drop this table
if ($run)
{
   --source suite/parts/inc/partition_layout_check1.inc
   # --source include/partition_layout_check1.inc
   eval DROP TABLE t1;
}
#### Try to create a table with the given subpartition number
--disable_abort_on_error
eval CREATE TABLE t1 (
$column_list
)
PARTITION BY RANGE(f_int1) SUBPARTITION BY HASH(f_int1)
SUBPARTITIONS $part_number
(PARTITION part1 VALUES LESS THAN ($max_row_div2),
 PARTITION part2 VALUES LESS THAN $MAX_VALUE);
--enable_abort_on_error
--disable_query_log
eval SET @my_errno= $mysql_errno ;
let $run= `SELECT @my_errno = 0`;
# Expected error codes are
# 0
# 1064 ERROR 42000: You have an error in your SQL syntax
#                   Reason: assign -1 subpartitions
# 1487 ERROR HY000: Too many partitions (including subpartitions) were defined
# 1492 ERROR HY000: Number of partitions = 0 is not an allowed value
let $unexpected_error= `SELECT @my_errno NOT IN (0,1064,1487,1492)`;
if ($unexpected_error)
{
   --echo #      The last command got an unexepected error response.
   --echo #      Expected/handled SQL codes are 0,1064,1487,1492
   SELECT '#      SQL code we got was: ' AS "", @my_errno AS "";
   --echo #      Sorry, have to abort.
   exit;
   --echo
}
--enable_query_log
#
# If this operation was successfull, print layout + drop this table
if ($run)
{
   --source suite/parts/inc/partition_layout_check1.inc
   # --source include/partition_layout_check1.inc
   eval DROP TABLE t1;
}
