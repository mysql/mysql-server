################################################################################
# inc/partition_alter_12.inc                                                   #
#                                                                              #
# Purpose:                                                                     #
#   Check ALTER partitioned table and the state of the table afterwards        #
#   The partitioning function use the PRIMARY KEY of the table KEY()           #
#                                                                              #
#   For partitioning methods                                                   #
#        PARTITION BY KEY                                                      #
#        Enable later : PARTITION BY RANGE/LIST ... SUBPARTITION BY KEY        #
#   do                                                                         #
#     1. Create the partitioned table                                          #
#     2. Test Alter Table that changes the key.                                #
#        - Insert rows into t1 to fill multiple partitions.                    #
#        - Execute the ALTER TABLE statement                                   #
#        - Check Table status                                                  #
#        - [Later] Execute the usability test include/partition_check.inc      #
#        - Drop the table t1                                                   #
#   done                                                                       #
#                                                                              #
#   The parameters                                                             #
#        $unique -- PRIMARY KEY or UNIQUE INDEXes to be created within the     #
#                   CREATE TABLE STATEMENT                                     #
#        $alter  -- ALTER TABLE statement, which has to be executed            #
#   have to be set before sourcing this routine.                               #
#   Example:                                                                   #
#          let $unique= , PRIMARY KEY;                                         #
#          let $alter= ALTER TABLE t1 DROP UNIQUE INDEX uidx1;                 #
#          inc/partition_alter1.inc                                            #
#                                                                              #
# Attention: This routine is created from include/partition_alter_11.inc       #
#            So if something has to be changed here it might be necessary to   #
#            do it also there.                                                 #
#------------------------------------------------------------------------------#
# Original Author: debanerj                                                    #
# Original Date: 2015-02-16                                                    #
# Change Author:                                                               #
# Change Date:                                                                 #
# Change:                                                                      #
################################################################################

--disable_warnings
DROP TABLE IF EXISTS t1;
--enable_warnings

let $partitioning= ;
--echo #----------- PARTITION BY KEY
if ($with_partitioning) {
  let $partitioning= PARTITION BY KEY() PARTITIONS 5;
}
eval CREATE TABLE t1 (
$column_list
$unique
)
$partitioning;

# The common framework partition_alter_1.inc uses same value/order for all the
# columns. This makes the distribution accross partition similar even after
# changing the partition key. For testing primary key() partition and  alter
# using local data.
#--source suite/parts/inc/partition_alter_1.inc

INSERT INTO t1(f_int1, f_int2) values(100, 200);
INSERT INTO t1(f_int1, f_int2) values(200, 300);
INSERT INTO t1(f_int1, f_int2) values(300, 400);
INSERT INTO t1(f_int1, f_int2) values(400, 500);
INSERT INTO t1(f_int1, f_int2) values(500, 100);

CHECK TABLE t1;

# Execute ALTER TABLE statement: Error
if ($expect_inplace_error) {
  --echo Execute ALTER: Expect Error 1845
  --error 1845
  eval $alter;
}

# Execute ALTER TABLE statement: Success
if ($expect_inplace_error == 0) {
  --echo Execute ALTER: Expect Success
  eval $alter;
}

CHECK TABLE t1;
SHOW CREATE TABLE t1;

DROP TABLE t1;

# SUBPARTITION BY KEY (unlike PARTITION BY KEY) does not currently support a
# default column. Enable when SUBPARTITION BY KEY() is supported.
if (0) {
--echo #----------- PARTITION BY RANGE -- SUBPARTITION BY KEY
if ($with_partitioning)
{
  let $partitioning= PARTITION BY RANGE(f_int1) SUBPARTITION BY KEY()
  (PARTITION part1 VALUES LESS THAN (0)
  (SUBPARTITION subpart11, SUBPARTITION subpart12),
  PARTITION part2 VALUES LESS THAN ($max_row_div4)
  (SUBPARTITION subpart21, SUBPARTITION subpart22),
  PARTITION part3 VALUES LESS THAN ($max_row_div2)
  (SUBPARTITION subpart31, SUBPARTITION subpart32),
  PARTITION part4 VALUES LESS THAN $MAX_VALUE
  (SUBPARTITION subpart41, SUBPARTITION subpart42));
}

eval CREATE TABLE t1 (
$column_list
$unique
)
$partitioning;

#Replace with local INSERT and CHECK as shown above
#--source suite/parts/inc/partition_alter_1.inc
DROP TABLE t1;

--echo #----------- PARTITION BY LIST -- SUBPARTITION BY KEY
if ($with_partitioning) {
  let $partitioning= PARTITION BY LIST(ABS(MOD(f_int1,2)))
  SUBPARTITION BY KEY() SUBPARTITIONS $sub_part_no
  (PARTITION part1 VALUES IN (0),
   PARTITION part2 VALUES IN (1),
   PARTITION part3 VALUES IN (NULL));
}

eval CREATE TABLE t1 (
$column_list
$unique
)
$partitioning;

#Replace with local INSERT and CHECK as shown above
#--source suite/parts/inc/partition_alter_1.inc
DROP TABLE t1;
}

