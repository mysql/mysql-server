################################################################################
# inc/partition_methods2.inc                                                   #
#                                                                              #
# Purpose:                                                                     #
#   Create and check partitioned tables                                        #
#   The partitioning function uses the columns f_int1 and f_int2               #
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
#          inc/partition_methods2.inc                                          #
#                                                                              #
# Attention: The routine inc/partition_methods1.inc is very similar            #
#            to this one. So if something has to be changed here it            #
#            might be necessary to do it also there                            #
#                                                                              #
#------------------------------------------------------------------------------#
# Original Author: mleich                                                      #
# Original Date: 2006-03-05                                                    #
# Change Author:                                                               #
# Change Date:                                                                 #
# Change:                                                                      #
################################################################################

--disable_warnings
DROP TABLE IF EXISTS t1;
--enable_warnings

let $partitioning= ;
#----------- PARTITION BY HASH
if ($with_partitioning)
{
let $partitioning= PARTITION BY HASH(f_int1 + f_int2) PARTITIONS 2;
}
eval CREATE TABLE t1 (
$column_list
$unique
)
$partitioning;
eval $insert_all;
--source suite/parts/inc/partition_check.inc
DROP TABLE t1;

#----------- PARTITION BY KEY
if ($with_partitioning)
{
let $partitioning= PARTITION BY KEY(f_int1,f_int2) PARTITIONS 5;
}
eval CREATE TABLE t1 (
$column_list
$unique
)
$partitioning;
eval $insert_all;
--source suite/parts/inc/partition_check.inc
DROP TABLE t1;

#----------- PARTITION BY LIST
if ($with_partitioning)
{
let $partitioning= PARTITION BY LIST(MOD(f_int1 + f_int2,4))
(PARTITION part_3 VALUES IN (-3),
 PARTITION part_2 VALUES IN (-2),
 PARTITION part_1 VALUES IN (-1),
 PARTITION part_N VALUES IN (NULL),
 PARTITION part0 VALUES IN (0),
 PARTITION part1 VALUES IN (1),
 PARTITION part2 VALUES IN (2),
 PARTITION part3 VALUES IN (3));
}
eval CREATE TABLE t1 (
$column_list
$unique
)
$partitioning;
eval $insert_all;
--source suite/parts/inc/partition_check.inc
DROP TABLE t1;

#----------- PARTITION BY RANGE
if ($with_partitioning)
{
--disable_query_log
eval SET @aux = 'PARTITION BY RANGE((f_int1 + f_int2) DIV 2)
(PARTITION parta VALUES LESS THAN (0),
PARTITION partb VALUES LESS THAN ($max_row_div4),
PARTITION partc VALUES LESS THAN ($max_row_div2),
PARTITION partd VALUES LESS THAN ($max_row_div2 + $max_row_div4),
PARTITION parte VALUES LESS THAN ($max_row),
PARTITION partf VALUES LESS THAN $MAX_VALUE)';
let $partitioning= `SELECT @aux`;
--enable_query_log
}
eval CREATE TABLE t1 (
$column_list
$unique
)
$partitioning;
eval $insert_all;
--source suite/parts/inc/partition_check.inc
DROP TABLE t1;

#----------- PARTITION BY RANGE -- SUBPARTITION BY HASH
if ($with_partitioning)
{
--disable_query_log
eval SET @aux =
'PARTITION BY RANGE(f_int1) SUBPARTITION BY HASH(f_int2) SUBPARTITIONS 2
(PARTITION parta VALUES LESS THAN (0),
PARTITION partb VALUES LESS THAN ($max_row_div4),
PARTITION partc VALUES LESS THAN ($max_row_div2),
PARTITION partd VALUES LESS THAN $MAX_VALUE)';
let $partitioning= `SELECT @aux`;
--enable_query_log
}
eval CREATE TABLE t1 (
$column_list
$unique
)
$partitioning;
eval $insert_all;
--source suite/parts/inc/partition_check.inc
DROP TABLE t1;

#----------- PARTITION BY RANGE -- SUBPARTITION BY KEY
if ($with_partitioning)
{
--disable_query_log
eval SET @aux = 'PARTITION BY RANGE(f_int1) SUBPARTITION BY KEY(f_int2)
(PARTITION part1 VALUES LESS THAN (0)
(SUBPARTITION subpart11, SUBPARTITION subpart12),
PARTITION part2 VALUES LESS THAN ($max_row_div4)
(SUBPARTITION subpart21, SUBPARTITION subpart22),
PARTITION part3 VALUES LESS THAN ($max_row_div2)
(SUBPARTITION subpart31, SUBPARTITION subpart32),
PARTITION part4 VALUES LESS THAN $MAX_VALUE
(SUBPARTITION subpart41, SUBPARTITION subpart42))';
let $partitioning= `SELECT @aux`;
--enable_query_log
}
eval CREATE TABLE t1 (
$column_list
$unique
)
$partitioning;
eval $insert_all;
--source suite/parts/inc/partition_check.inc
DROP TABLE t1;

#----------- PARTITION BY LIST -- SUBPARTITION BY HASH
if ($with_partitioning)
{
let $partitioning= PARTITION BY LIST(ABS(MOD(f_int1,3))) SUBPARTITION BY HASH(f_int2 + 1)
(PARTITION part1 VALUES IN (0)
  (SUBPARTITION sp11, SUBPARTITION sp12),
 PARTITION part2 VALUES IN (1)
  (SUBPARTITION sp21, SUBPARTITION sp22),
 PARTITION part3 VALUES IN (2)
  (SUBPARTITION sp31, SUBPARTITION sp32),
 PARTITION part4 VALUES IN (NULL)
  (SUBPARTITION sp41, SUBPARTITION sp42));
}
eval CREATE TABLE t1 (
$column_list
$unique
)
$partitioning;
eval $insert_all;
--source suite/parts/inc/partition_check.inc
DROP TABLE t1;

#----------- PARTITION BY LIST -- SUBPARTITION BY KEY
if ($with_partitioning)
{
--disable_query_log
eval SET @aux =
'PARTITION BY LIST(ABS(MOD(f_int1,2)))
SUBPARTITION BY KEY(f_int2)  SUBPARTITIONS $sub_part_no
(PARTITION part1 VALUES IN (0),
 PARTITION part2 VALUES IN (1),
 PARTITION part3 VALUES IN (NULL))';
let $partitioning= `SELECT @aux`;
--enable_query_log
}
eval CREATE TABLE t1 (
$column_list
$unique
)
$partitioning;
eval $insert_all;
--source suite/parts/inc/partition_check.inc
DROP TABLE t1;
