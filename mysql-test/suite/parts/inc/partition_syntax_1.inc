################################################################################
# inc/partition_syntax_1.inc                                                   #
#                                                                              #
# Purpose:                                                                     #
#   Auxiliary script, only useful when sourced by inc/partition_syntax.inc     #
#                                                                              #
#    Try to create a table with number of partitions/subpartitions             #
#    = $part_number. Print the layout of the table and drop it.                #
#                                                                              #
#    The parameter $part_number must be set before sourcing this script.       #
#                                                                              #
#------------------------------------------------------------------------------#
# Original Author: mleich                                                      #
# Original Date:   2006-03-05                                                  #
# Change Author:   mleich                                                      #
# Change Date:     2007-10-08                                                  #
# Change:          Fix for                                                     #
#                  Bug#31481 test suite parts: Many tests fail because of      #
#                            changed server error codes                        #
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
# 0, ER_PARSE_ERROR (Reason: assign -1 partitions), ER_TOO_MANY_PARTITIONS_ERROR
# and ER_NO_PARTS_ERROR
if (`SELECT @my_errno NOT IN (0,$ER_PARSE_ERROR,$ER_TOO_MANY_PARTITIONS_ERROR,
                              $ER_NO_PARTS_ERROR)`)
{
   --echo #      The last command got an unexepected error response.
   --echo #      Expected/handled SQL codes are 0,$ER_PARSE_ERROR,$ER_TOO_MANY_PARTITIONS_ERROR,$ER_NO_PARTS_ERROR
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
# 0, ER_PARSE_ERROR (Reason: assign -1 partitions), ER_TOO_MANY_PARTITIONS_ERROR
# and ER_NO_PARTS_ERROR
if (`SELECT @my_errno NOT IN (0,$ER_PARSE_ERROR,$ER_TOO_MANY_PARTITIONS_ERROR,
                              $ER_NO_PARTS_ERROR)`)
{
   --echo #      The last command got an unexepected error response.
   --echo #      Expected/handled SQL codes are 0,$ER_PARSE_ERROR,$ER_TOO_MANY_PARTITIONS_ERROR,$ER_NO_PARTS_ERROR
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
   eval DROP TABLE t1;
}
