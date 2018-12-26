################################################################################
# inc/partition_methods1.inc                                                   #
#                                                                              #
# Purpose:                                                                     #
#   Create and check partitioned tables                                        #
#   The partitioning function use the column f_int1                            #
#                                                                              #
#   For all partitioning methods                                               #
#        PARTITION BY HASH/KEY/LIST/RANGE                                      #
#        PARTITION BY RANGE/LIST ... SUBPARTITION BY HASH/KEY ...              #
#   do                                                                         #
#     1. Create the partitioned table                                          #
#     2  Insert the content of the table t0_template into t1                   #
#     3. Execute inc/partition_check.inc                                       #
#     4. Drop the table t1                                                     #
#   done                                                                       #
#                                                                              #
#   The parameter                                                              #
#        $unique -- PRIMARY KEY or UNIQUE INDEXes to be created within the     #
#                   CREATE TABLE STATEMENT                                     #
#   has to be set before sourcing this routine.                                #
#   Example:                                                                   #
#          let $unique= , UNIQUE INDEX uidx1 (f_int1);                         #
#          inc/partition_method1s.inc                                          #
#                                                                              #
# Attention: The routine inc/partition_methods2.inc is very similar            #
#            to this one. So if something has to be changed here it            #
#            might be necessary to do it also there                            #
#                                                                              #
#------------------------------------------------------------------------------#
# Original Author: mleich                                                      #
# Original Date: 2006-03-05                                                    #
# Change Author: HH                                                            #
# Change Date:   2006-05-12                                                    #
# Change:        Introduced DATA/INDEX DIRECTORY                               #
################################################################################

--disable_warnings
DROP TABLE IF EXISTS t1;
--enable_warnings

let $partitioning= ;
#----------- PARTITION BY HASH
if ($with_partitioning)
{
let $partitioning= PARTITION BY HASH(f_int1) PARTITIONS 2;
if ($with_directories)
{
let $partitioning=
PARTITION BY HASH(f_int1) PARTITIONS 2
(PARTITION p1 
$data_directory 
$index_directory,
PARTITION p2 
$data_directory 
$index_directory);
}
}
--replace_result $MYSQLTEST_VARDIR MYSQLTEST_VARDIR
eval CREATE TABLE t1 (
$column_list
$unique
)
$partitioning;
eval $insert_all;
--source suite/parts/inc/partition_check.inc
DROP TABLE t1;
--source suite/parts/inc/partition_check_drop.inc

#----------- PARTITION BY KEY
if ($with_partitioning)
{
let $partitioning= 
PARTITION BY KEY(f_int1) PARTITIONS 5;
if ($with_directories)
{
let $partitioning= 
PARTITION BY KEY(f_int1) PARTITIONS 5
(PARTITION p1
$data_directory
$index_directory,
PARTITION p2
$data_directory
$index_directory,
PARTITION p3
$data_directory
$index_directory,
PARTITION p4
$data_directory
$index_directory,
PARTITION p5
$data_directory
$index_directory);
}
}
--replace_result $MYSQLTEST_VARDIR MYSQLTEST_VARDIR
eval CREATE TABLE t1 (
$column_list
$unique
)
$partitioning;
eval $insert_all;
--source suite/parts/inc/partition_check.inc
DROP TABLE t1;
--source suite/parts/inc/partition_check_drop.inc

#----------- PARTITION BY LIST
if ($with_partitioning)
{
let $partitioning= 
PARTITION BY LIST(MOD(f_int1,4))
(PARTITION part_3 VALUES IN (-3),
 PARTITION part_2 VALUES IN (-2),
 PARTITION part_1 VALUES IN (-1),
 PARTITION part_N VALUES IN (NULL),
 PARTITION part0 VALUES IN (0),
 PARTITION part1 VALUES IN (1),
 PARTITION part2 VALUES IN (2),
 PARTITION part3 VALUES IN (3));
if ($with_directories)
{
let $partitioning= 
PARTITION BY LIST(MOD(f_int1,4))
(PARTITION part_3 VALUES IN (-3)
$data_directory $index_directory,
 PARTITION part_2 VALUES IN (-2)
$data_directory $index_directory,
 PARTITION part_1 VALUES IN (-1)
$data_directory $index_directory,
 PARTITION part_N VALUES IN (NULL)
$data_directory $index_directory,
 PARTITION part0 VALUES IN (0)
$data_directory $index_directory,
 PARTITION part1 VALUES IN (1)
$data_directory $index_directory,
 PARTITION part2 VALUES IN (2)
$data_directory $index_directory,
 PARTITION part3 VALUES IN (3)
$data_directory $index_directory);
}
}
--replace_result $MYSQLTEST_VARDIR MYSQLTEST_VARDIR
eval CREATE TABLE t1 (
$column_list
$unique
)
$partitioning;
eval $insert_all;
--source suite/parts/inc/partition_check.inc
DROP TABLE t1;
--source suite/parts/inc/partition_check_drop.inc

#----------- PARTITION BY RANGE
if ($with_partitioning)
{
let $partitioning= PARTITION BY RANGE(f_int1)
(PARTITION parta VALUES LESS THAN (0),
PARTITION partb VALUES LESS THAN ($max_row_div4),
PARTITION partc VALUES LESS THAN ($max_row_div2),
PARTITION partd VALUES LESS THAN ($max_row_div2 + $max_row_div4),
PARTITION parte VALUES LESS THAN ($max_row),
PARTITION partf VALUES LESS THAN $MAX_VALUE);
if ($with_directories)
{
let $partitioning= PARTITION BY RANGE(f_int1)
(PARTITION parta VALUES LESS THAN (0)
$data_directory
$index_directory,
PARTITION partb VALUES LESS THAN ($max_row_div4)
$data_directory
$index_directory,
PARTITION partc VALUES LESS THAN ($max_row_div2)
$data_directory
$index_directory,
PARTITION partd VALUES LESS THAN ($max_row_div2 + $max_row_div4)
$data_directory
$index_directory,
PARTITION parte VALUES LESS THAN ($max_row)
$data_directory
$index_directory,
PARTITION partf VALUES LESS THAN $MAX_VALUE
$data_directory
$index_directory);
}
}
--replace_result $MYSQLTEST_VARDIR MYSQLTEST_VARDIR
eval CREATE TABLE t1 (
$column_list
$unique
)
$partitioning;
eval $insert_all;
--source suite/parts/inc/partition_check.inc
DROP TABLE t1;
--source suite/parts/inc/partition_check_drop.inc

#----------- PARTITION BY RANGE -- SUBPARTITION BY HASH
if ($with_partitioning)
{
let $partitioning= 
PARTITION BY RANGE(f_int1 DIV 2) SUBPARTITION BY HASH(f_int1) SUBPARTITIONS 2
(PARTITION parta VALUES LESS THAN (0),
PARTITION partb VALUES LESS THAN ($max_row_div4),
PARTITION partc VALUES LESS THAN ($max_row_div2),
PARTITION partd VALUES LESS THAN $MAX_VALUE);
if ($with_directories)
{
let $partitioning= 
PARTITION BY RANGE(f_int1 DIV 2) SUBPARTITION BY HASH(f_int1) SUBPARTITIONS 2
(PARTITION parta VALUES LESS THAN (0)
$data_directory
$index_directory,
PARTITION partb VALUES LESS THAN ($max_row_div4)
$data_directory
$index_directory,
PARTITION partc VALUES LESS THAN ($max_row_div2)
$data_directory
$index_directory,
PARTITION partd VALUES LESS THAN $MAX_VALUE
$data_directory
$index_directory);
}
}
--replace_result $MYSQLTEST_VARDIR MYSQLTEST_VARDIR
eval CREATE TABLE t1 (
$column_list
$unique
)
$partitioning;
eval $insert_all;
--source suite/parts/inc/partition_check.inc
DROP TABLE t1;
--source suite/parts/inc/partition_check_drop.inc

#----------- PARTITION BY RANGE -- SUBPARTITION BY KEY
if ($with_partitioning)
{
let $partitioning= PARTITION BY RANGE(f_int1) SUBPARTITION BY KEY(f_int1)
(PARTITION part1 VALUES LESS THAN (0)
(SUBPARTITION subpart11, SUBPARTITION subpart12),
PARTITION part2 VALUES LESS THAN ($max_row_div4)
(SUBPARTITION subpart21, SUBPARTITION subpart22),
PARTITION part3 VALUES LESS THAN ($max_row_div2)
(SUBPARTITION subpart31, SUBPARTITION subpart32),
PARTITION part4 VALUES LESS THAN $MAX_VALUE
(SUBPARTITION subpart41, SUBPARTITION subpart42));
if ($with_directories)
{
let $partitioning= PARTITION BY RANGE(f_int1) SUBPARTITION BY KEY(f_int1)
(PARTITION part1 VALUES LESS THAN (0)
(SUBPARTITION subpart11 $data_directory $index_directory,
 SUBPARTITION subpart12 $data_directory $index_directory),
PARTITION part2 VALUES LESS THAN ($max_row_div4)
(SUBPARTITION subpart21 $data_directory $index_directory,
 SUBPARTITION subpart22 $data_directory $index_directory),
PARTITION part3 VALUES LESS THAN ($max_row_div2)
(SUBPARTITION subpart31 $data_directory $index_directory,
 SUBPARTITION subpart32 $data_directory $index_directory),
PARTITION part4 VALUES LESS THAN $MAX_VALUE
(SUBPARTITION subpart41 $data_directory $index_directory,
 SUBPARTITION subpart42 $data_directory $index_directory));
}
}
--replace_result $MYSQLTEST_VARDIR MYSQLTEST_VARDIR
eval CREATE TABLE t1 (
$column_list
$unique
)
$partitioning;
eval $insert_all;
--source suite/parts/inc/partition_check.inc
DROP TABLE t1;
--source suite/parts/inc/partition_check_drop.inc

#----------- PARTITION BY LIST -- SUBPARTITION BY HASH
if ($with_partitioning)
{
let $partitioning= 
PARTITION BY LIST(ABS(MOD(f_int1,3))) SUBPARTITION BY HASH(f_int1 + 1)
(PARTITION part1 VALUES IN (0)
  (SUBPARTITION sp11,
   SUBPARTITION sp12),
 PARTITION part2 VALUES IN (1)
  (SUBPARTITION sp21,
   SUBPARTITION sp22),
 PARTITION part3 VALUES IN (2)
  (SUBPARTITION sp31, 
   SUBPARTITION sp32),
 PARTITION part4 VALUES IN (NULL)
  (SUBPARTITION sp41,
   SUBPARTITION sp42));
if ($with_directories)
{
let $partitioning= 
PARTITION BY LIST(ABS(MOD(f_int1,3))) SUBPARTITION BY HASH(f_int1 + 1)
(PARTITION part1 VALUES IN (0)
  $data_directory
  $index_directory
  (SUBPARTITION sp11 
     $data_directory
     $index_directory,
   SUBPARTITION sp12
     $data_directory
     $index_directory),
 PARTITION part2 VALUES IN (1)
  $data_directory
  $index_directory
  (SUBPARTITION sp21 
     $data_directory
     $index_directory,
   SUBPARTITION sp22
     $data_directory
     $index_directory),
 PARTITION part3 VALUES IN (2)
  $data_directory
  $index_directory
  (SUBPARTITION sp31, 
   SUBPARTITION sp32),
 PARTITION part4 VALUES IN (NULL)
  $data_directory
  $index_directory
  (SUBPARTITION sp41 
     $data_directory
     $index_directory,
   SUBPARTITION sp42
     $data_directory
     $index_directory));
}
}
--replace_result $MYSQLTEST_VARDIR MYSQLTEST_VARDIR
eval CREATE TABLE t1 (
$column_list
$unique
)
$partitioning;
eval $insert_all;
--source suite/parts/inc/partition_check.inc
DROP TABLE t1;
--source suite/parts/inc/partition_check_drop.inc

#----------- PARTITION BY LIST -- SUBPARTITION BY KEY
if ($with_partitioning)
{
let $partitioning= 
PARTITION BY LIST(ABS(MOD(f_int1,2)))
SUBPARTITION BY KEY(f_int1) SUBPARTITIONS $sub_part_no
(PARTITION part1 VALUES IN (0),
 PARTITION part2 VALUES IN (1),
 PARTITION part3 VALUES IN (NULL));
if ($with_directories)
{
let $partitioning= 
PARTITION BY LIST(ABS(MOD(f_int1,2)))
SUBPARTITION BY KEY(f_int1) SUBPARTITIONS $sub_part_no
(PARTITION part1 VALUES IN (0)
  $data_directory
  $index_directory,
 PARTITION part2 VALUES IN (1)
  $data_directory
  $index_directory,
 PARTITION part3 VALUES IN (NULL)
  $data_directory
  $index_directory);
}
}
--replace_result $MYSQLTEST_VARDIR MYSQLTEST_VARDIR
eval CREATE TABLE t1 (
$column_list
$unique
)
$partitioning;
eval $insert_all;
--source suite/parts/inc/partition_check.inc
DROP TABLE t1;
--source suite/parts/inc/partition_check_drop.inc
