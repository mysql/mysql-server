# inc/partition_11.inc
#
#    Try to create a table with the given partition number
#

eval CREATE TABLE t1 ( f1 INTEGER, f2 char(20))
PARTITION BY HASH(f1) PARTITIONS $part_number;
--disable_query_log
eval SET @my_errno= $mysql_errno ;
let $run= `SELECT @my_errno = 0`;
--enable_query_log
#
# If this operation was successfull, check + drop this table
if ($run)
{
   --source suite/parts/inc/partition_10.inc
   eval DROP TABLE t1;
}
#### Try to create a table with the given subpartition number
eval CREATE TABLE t1 ( f1 INTEGER, f2 char(20))
PARTITION    BY RANGE(f1) SUBPARTITION BY HASH(f1)
SUBPARTITIONS $part_number
(PARTITION part1 VALUES LESS THAN ($max_row_div2), PARTITION part2 VALUES LESS THAN ($max_int_4));
--disable_query_log
eval SET @my_errno= $mysql_errno ;
let $run= `SELECT @my_errno = 0`;
--enable_query_log
#
# If this operation was successfull, check + drop this table
if ($run)
{
   --source suite/parts/inc/partition_10.inc
   eval DROP TABLE t1;
}
