################################################################################
# inc/partition_syntax.inc                                                     #
#                                                                              #
# Purpose:                                                                     #
#   Tests around Create partitioned tables syntax                              #
#                                                                              #
#------------------------------------------------------------------------------#
# Original Author: mleich                                                      #
# Original Date: 2006-03-05                                                    #
# Change Author:                                                               #
# Change Date:                                                                 #
# Change:                                                                      #
################################################################################

#  FIXME Implement testcases, where it is checked that all create and
#  alter table statements
#  - with missing mandatory parameters are rejected
#  - with optional parameters are accepted
#  - with wrong combinations of optional parameters are rejected
#  - ............

--echo
--echo #========================================================================
--echo #  1.    Any PRIMARY KEYs or UNIQUE INDEXes must contain the columns used
--echo #        within the partitioning functions
--echo #========================================================================
--disable_warnings
DROP TABLE IF EXISTS t1;
--enable_warnings
#
--echo #------------------------------------------------------------------------
--echo #  1.1 column of partitioning function not included in PRIMARY KEY
--echo #               PARTITION BY HASH/KEY/LIST/RANGE
--echo #------------------------------------------------------------------------
#----------- PARTITION BY HASH
--error ER_UNIQUE_KEY_NEED_ALL_FIELDS_IN_PF
eval CREATE TABLE t1 (
$column_list,
PRIMARY KEY (f_int2)
)
PARTITION BY HASH(f_int1) PARTITIONS 2;
--error ER_UNIQUE_KEY_NEED_ALL_FIELDS_IN_PF
eval CREATE TABLE t1 (
$column_list,
PRIMARY KEY (f_int2)
)
PARTITION BY HASH(f_int1 + f_int2) PARTITIONS 2;
#----------- PARTITION BY KEY
--error ER_UNIQUE_KEY_NEED_ALL_FIELDS_IN_PF
eval CREATE TABLE t1 (
$column_list,
PRIMARY KEY (f_int2)
)
PARTITION BY KEY(f_int1) PARTITIONS 2;
--error ER_UNIQUE_KEY_NEED_ALL_FIELDS_IN_PF
eval CREATE TABLE t1 (
$column_list,
PRIMARY KEY (f_int2)
)
PARTITION BY KEY(f_int1,f_int2) PARTITIONS 2;
#----------- PARTITION BY LIST
--error ER_UNIQUE_KEY_NEED_ALL_FIELDS_IN_PF
eval CREATE TABLE t1 (
$column_list,
PRIMARY KEY (f_int2)
)
PARTITION BY LIST(f_int1)
(PARTITION part1 VALUES IN (1));
--error ER_UNIQUE_KEY_NEED_ALL_FIELDS_IN_PF
eval CREATE TABLE t1 (
$column_list,
PRIMARY KEY (f_int2)
)
PARTITION BY LIST(f_int1 + f_int2)
(PARTITION part1 VALUES IN (1));
#----------- PARTITION BY RANGE
--error ER_UNIQUE_KEY_NEED_ALL_FIELDS_IN_PF
eval CREATE TABLE t1 (
$column_list,
PRIMARY KEY (f_int2)
)
PARTITION BY RANGE(f_int1)
(PARTITION part1 VALUES LESS THAN (1));
--error ER_UNIQUE_KEY_NEED_ALL_FIELDS_IN_PF
eval CREATE TABLE t1 (
$column_list,
PRIMARY KEY (f_int2)
)
PARTITION BY RANGE(f_int1 + f_int2)
(PARTITION part1 VALUES LESS THAN (1));

#
--echo #------------------------------------------------------------------------
--echo #  1.2 column of partitioning function not included in UNIQUE INDEX
--echo #               PARTITION BY HASH/KEY/LIST/RANGE
--echo #      Variant a) Without additional PRIMARY KEY        
--echo #      Variant b) With correct additional PRIMARY KEY        
--echo #      Variant 1) one column in partitioning function
--echo #      Variant 2) two columns in partitioning function
--echo #------------------------------------------------------------------------
#      Note: If the CREATE TABLE statement contains no PRIMARY KEY but
#            UNIQUE INDEXes the MySQL layer tells the storage to use
#            the first UNIQUE INDEX as PRIMARY KEY.

let $unique_index= UNIQUE INDEX (f_int2);

#----------- PARTITION BY HASH
let $partition_scheme= PARTITION BY HASH(f_int1) PARTITIONS 2;
--source suite/parts/inc/partition_syntax_2.inc
let $partition_scheme= PARTITION BY HASH(f_int1 + f_int2) PARTITIONS 2;
--source suite/parts/inc/partition_syntax_2.inc
#----------- PARTITION BY KEY
let $partition_scheme= PARTITION BY KEY(f_int1) PARTITIONS 2;
--source suite/parts/inc/partition_syntax_2.inc
let $partition_scheme= PARTITION BY KEY(f_int1,f_int2) PARTITIONS 2;
--source suite/parts/inc/partition_syntax_2.inc
#----------- PARTITION BY LIST
let $partition_scheme= PARTITION BY LIST(MOD(f_int1,3))
   (PARTITION partN VALUES IN (NULL),
    PARTITION part0 VALUES IN (0),
    PARTITION part1 VALUES IN (1),
    PARTITION part2 VALUES IN (2));
--source suite/parts/inc/partition_syntax_2.inc
let $partition_scheme= PARTITION BY LIST(MOD(f_int1 + f_int2,3))
   (PARTITION partN VALUES IN (NULL),
    PARTITION part0 VALUES IN (0),
    PARTITION part1 VALUES IN (1),
    PARTITION part2 VALUES IN (2));
--source suite/parts/inc/partition_syntax_2.inc
#----------- PARTITION BY RANGE
let $partition_scheme= PARTITION BY RANGE(f_int1)
   (PARTITION part1 VALUES LESS THAN (1),
    PARTITION part2 VALUES LESS THAN (2147483646));
--source suite/parts/inc/partition_syntax_2.inc
let $partition_scheme= PARTITION BY RANGE(f_int1 + f_int2)
   (PARTITION part1 VALUES LESS THAN (1),
    PARTITION part2 VALUES LESS THAN (2147483646));
--source suite/parts/inc/partition_syntax_2.inc

#
--echo #------------------------------------------------------------------------
--echo #  1.3 column of subpartitioning function not included in PRIMARY KEY
--echo #               PARTITION BY RANGE/LIST -- SUBPARTITION BY HASH/KEY
--echo #------------------------------------------------------------------------

#----------- PARTITION BY RANGE -- SUBPARTITION BY HASH
--error ER_UNIQUE_KEY_NEED_ALL_FIELDS_IN_PF
eval CREATE TABLE t1 (
$column_list,
PRIMARY KEY (f_int2)
)
PARTITION BY RANGE(f_int2) SUBPARTITION BY HASH(f_int1)
(PARTITION part1 VALUES LESS THAN (1)
   (SUBPARTITION subpart1));
#----------- PARTITION BY RANGE -- SUBPARTITION BY KEY
--error ER_UNIQUE_KEY_NEED_ALL_FIELDS_IN_PF
eval CREATE TABLE t1 (
$column_list,
PRIMARY KEY (f_int2)
)
PARTITION BY RANGE(f_int2) SUBPARTITION BY KEY(f_int1)
(PARTITION part1 VALUES LESS THAN (1)
   (SUBPARTITION subpart1));
#----------- PARTITION BY LIST -- SUBPARTITION BY HASH
--error ER_UNIQUE_KEY_NEED_ALL_FIELDS_IN_PF
eval CREATE TABLE t1 (
$column_list,
PRIMARY KEY (f_int2)
)
PARTITION BY LIST(f_int2) SUBPARTITION BY HASH(f_int1)
(PARTITION part1 VALUES IN (1)
   (SUBPARTITION subpart1));
#----------- PARTITION BY LIST -- SUBPARTITION BY KEY
--error ER_UNIQUE_KEY_NEED_ALL_FIELDS_IN_PF
eval CREATE TABLE t1 (
$column_list,
PRIMARY KEY (f_int2)
)
PARTITION BY LIST(f_int2) SUBPARTITION BY KEY(f_int1)
(PARTITION part1 VALUES IN (1)
   (SUBPARTITION subpart1));

#
--echo #------------------------------------------------------------------------
--echo #  1.4 column of subpartitioning function not included in UNIQUE INDEX
--echo #               PARTITION BY RANGE/LIST -- SUBPARTITION BY HASH/KEY
--echo #      Variant a) Without additional PRIMARY KEY        
--echo #      Variant b) With correct additional PRIMARY KEY        
--echo #------------------------------------------------------------------------
let $partition_scheme= PARTITION BY RANGE(f_int2)
SUBPARTITION BY HASH(f_int1) SUBPARTITIONS 3
   (PARTITION part1 VALUES LESS THAN (1),
    PARTITION part2 VALUES LESS THAN (2147483646));
--source suite/parts/inc/partition_syntax_2.inc
#----------- PARTITION BY RANGE -- SUBPARTITION BY KEY
let $partition_scheme= PARTITION BY RANGE(f_int2)
SUBPARTITION BY KEY(f_int1) SUBPARTITIONS 3
   (PARTITION part1 VALUES LESS THAN (1),
    PARTITION part2 VALUES LESS THAN (2147483646));
--source suite/parts/inc/partition_syntax_2.inc
#----------- PARTITION BY LIST -- SUBPARTITION BY HASH
let $partition_scheme= PARTITION BY LIST(MOD(f_int2,3))
SUBPARTITION BY HASH(f_int1) SUBPARTITIONS 2
   (PARTITION partN VALUES IN (NULL),
    PARTITION part0 VALUES IN (0),
    PARTITION part1 VALUES IN (1),
    PARTITION part2 VALUES IN (2));
--source suite/parts/inc/partition_syntax_2.inc
#----------- PARTITION BY LIST -- SUBPARTITION BY KEY
let $partition_scheme= PARTITION BY LIST(MOD(f_int2,3))
SUBPARTITION BY KEY(f_int1) SUBPARTITIONS 2
   (PARTITION partN VALUES IN (NULL),
    PARTITION part0 VALUES IN (0),
    PARTITION part1 VALUES IN (1),
    PARTITION part2 VALUES IN (2));
--source suite/parts/inc/partition_syntax_2.inc

--echo
--echo #========================================================================
--echo #  2   Some properties around subpartitioning
--echo #========================================================================
--echo #------------------------------------------------------------------------
--echo #  2.1 Subpartioned table without subpartitioning rule must be rejected
--echo #------------------------------------------------------------------------
--disable_warnings
DROP TABLE IF EXISTS t1;
--enable_warnings
# Bug#15961 Partitions: Creation of subpart. table without subpart. rule not rejected
--error ER_SUBPARTITION_ERROR
eval CREATE TABLE t1 (
$column_list
)
PARTITION BY RANGE(f_int1)
( PARTITION part1 VALUES LESS THAN (1000) (SUBPARTITION subpart11));
--echo #------------------------------------------------------------------------
--echo #  2.2 Every partition must have the same number of subpartitions.
--echo #      This is a limitation of MySQL 5.1, which could be removed in
--echo #      later releases.
--echo #------------------------------------------------------------------------
--error ER_PARSE_ERROR
eval CREATE TABLE t1 (
$column_list,
PRIMARY KEY (f_int1)
)
PARTITION BY RANGE(f_int1) SUBPARTITION BY KEY(f_int1)
(
 PARTITION part1 VALUES LESS THAN (0)
     (SUBPARTITION subpart1),
 PARTITION part2 VALUES LESS THAN ($max_row_div4)
     (SUBPARTITION subpart1, SUBPARTITION subpart2));

--echo
--echo #========================================================================
--echo #  3   VALUES clauses
--echo #========================================================================
--echo #------------------------------------------------------------------------
--echo #  3.1 The constants in VALUES IN clauses must differ
--echo #------------------------------------------------------------------------
--error ER_MULTIPLE_DEF_CONST_IN_LIST_PART_ERROR
eval CREATE TABLE t1 (
$column_list
)
PARTITION BY LIST(MOD(f_int1,2))
( PARTITION part1 VALUES IN (-1),
  PARTITION part2 VALUES IN (0),
  PARTITION part3 VALUES IN (-1));
# constant followed by the same constant
--error ER_RANGE_NOT_INCREASING_ERROR
CREATE TABLE t1 (f1 BIGINT, f2 BIGINT)
PARTITION BY RANGE(f1)
(PARTITION part1 VALUES LESS THAN (0),
PARTITION part2 VALUES LESS THAN (0),
PARTITION part3 VALUES LESS THAN (10000));

--echo #------------------------------------------------------------------------
--echo #  3.2 The constants in VALUES LESS must be in increasing order
--echo #------------------------------------------------------------------------
# constant followed somewhere by the smaller constant
--error ER_RANGE_NOT_INCREASING_ERROR
CREATE TABLE t1 (f1 BIGINT, f2 BIGINT)
PARTITION BY RANGE(f1)
(PARTITION part1 VALUES LESS THAN (0),
PARTITION part2 VALUES LESS THAN (-1),
PARTITION part3 VALUES LESS THAN (10000));

--echo #------------------------------------------------------------------------
--echo #  3.3 LIST partitions must be defined with VALUES IN
--echo #------------------------------------------------------------------------
--error ER_PARTITION_WRONG_VALUES_ERROR
eval CREATE TABLE t1 (
$column_list
)
PARTITION BY LIST(MOD(f_int1,2))
( PARTITION part1 VALUES LESS THAN (-1),
  PARTITION part2 VALUES LESS THAN (0),
  PARTITION part3 VALUES LESS THAN (1000));

--echo #------------------------------------------------------------------------
--echo #  3.4 RANGE partitions must be defined with VALUES LESS THAN
--echo #------------------------------------------------------------------------
--error ER_PARTITION_WRONG_VALUES_ERROR
eval CREATE TABLE t1 (
$column_list
)
PARTITION BY RANGE(f_int1)
( PARTITION part1 VALUES IN (-1),
  PARTITION part2 VALUES IN (0),
  PARTITION part3 VALUES IN (1000));

--echo #------------------------------------------------------------------------
--echo #  3.5 Use of NULL in VALUES clauses
--echo #------------------------------------------------------------------------
--echo #  3.5.1 NULL in RANGE partitioning clause
--echo #  3.5.1.1 VALUE LESS THAN (NULL) is not allowed
--error ER_NULL_IN_VALUES_LESS_THAN
eval CREATE TABLE t1 (
$column_list
)
PARTITION BY RANGE(f_int1)
( PARTITION part1 VALUES LESS THAN (NULL),
  PARTITION part2 VALUES LESS THAN (1000));
--echo #  3.5.1.2 VALUE LESS THAN (NULL) is not allowed
--error ER_NULL_IN_VALUES_LESS_THAN
eval CREATE TABLE t1 (
$column_list
)
PARTITION BY RANGE(f_int1)
( PARTITION part1 VALUES LESS THAN (NULL),
  PARTITION part2 VALUES LESS THAN (1000));
--echo #  3.5.2 NULL in LIST partitioning clause
--echo #  3.5.2.1 VALUE IN (NULL)
eval CREATE TABLE t1 (
$column_list
)
PARTITION BY LIST(MOD(f_int1,2))
( PARTITION part1 VALUES IN (NULL),
  PARTITION part2 VALUES IN (0),
  PARTITION part3 VALUES IN (1));
DROP TABLE t1;
--echo #  3.5.2.2 VALUE IN (NULL)
# Attention: It is intended that there is no partition with
#            VALUES IN (0), because there was a time where NULL was treated as zero
eval CREATE TABLE t1 (
$column_list
)
PARTITION BY LIST(MOD(f_int1,2))
( PARTITION part1 VALUES IN (NULL),
  PARTITION part3 VALUES IN (1));
--source suite/parts/inc/partition_layout_check1.inc
DROP TABLE t1;
--echo #  3.5.3 Reveal that IN (...NULL) is not mapped to IN(0)
# Bug#15447: Partitions: NULL is treated as zero
# We would get a clash here if such a mapping would be done.
eval CREATE TABLE t1 (
$column_list
)
PARTITION BY LIST(MOD(f_int1,2))
( PARTITION part1 VALUES IN (NULL),
  PARTITION part2 VALUES IN (0),
  PARTITION part3 VALUES IN (1));
--source suite/parts/inc/partition_layout_check1.inc
DROP TABLE t1;

# FIXME Implement some non integer constant tests


--echo
--echo #========================================================================
--echo #  4. Check assigning the number of partitions and subpartitions
--echo #     with and without named partitions/subpartitions
--echo #========================================================================
--disable_warnings
DROP TABLE IF EXISTS t1;
--enable_warnings
--echo #------------------------------------------------------------------------
--echo # 4.1 (positive) without partition/subpartition number assignment
--echo #------------------------------------------------------------------------
--echo # 4.1.1 no partition number, no named partitions
eval CREATE TABLE t1 (
$column_list
)
PARTITION BY HASH(f_int1);
--source suite/parts/inc/partition_layout_check1.inc
DROP TABLE t1;
--echo # 4.1.2 no partition number, named partitions
eval CREATE TABLE t1 (
$column_list
)
PARTITION BY HASH(f_int1) (PARTITION part1, PARTITION part2);
--source suite/parts/inc/partition_layout_check1.inc
DROP TABLE t1;
# Attention: Several combinations are impossible
#           If subpartitioning exists
#           - partitioning algorithm must be RANGE or LIST
#             This implies the assignment of named partitions.
#           - subpartitioning algorithm must be HASH or KEY
--echo # 4.1.3 variations on no partition/subpartition number, named partitions,
--echo #       different subpartitions are/are not named
#
# Partition name   -- "properties"
#    part1         --     first/non last
#    part2         -- non first/non last
#    part3         -- non first/    last
#
# Testpattern:
#         named subpartitions in
# Partition part1 part2 part3
#             N     N     N
#             N     N     Y
#             N     Y     N
#             N     Y     Y
#             Y     N     N
#             Y     N     Y
#             Y     Y     N
#             Y     Y     Y
--disable_query_log
let $part01= CREATE TABLE t1 ( ;
let $part02= )
PARTITION BY RANGE(f_int1) SUBPARTITION BY HASH(f_int1);
#
eval SET @aux = '(PARTITION part1 VALUES LESS THAN ($max_row_div2),';
let $part1_N= `SELECT @AUX`;
eval SET @aux = '(PARTITION part1 VALUES LESS THAN ($max_row_div2)
(SUBPARTITION subpart11 , SUBPARTITION subpart12 ),';
let $part1_Y= `SELECT @AUX`;
#
eval SET @aux = 'PARTITION part2 VALUES LESS THAN ($max_row),';
let $part2_N= `SELECT @AUX`;
eval SET @aux = 'PARTITION part2 VALUES LESS THAN ($max_row)
(SUBPARTITION subpart21 , SUBPARTITION subpart22 ),';
let $part2_Y= `SELECT @AUX`;
#
eval SET @aux = 'PARTITION part3 VALUES LESS THAN $MAX_VALUE)';
let $part3_N= `SELECT @AUX`;
eval SET @aux = 'PARTITION part3 VALUES LESS THAN $MAX_VALUE
(SUBPARTITION subpart31 , SUBPARTITION subpart32 ))';
let $part3_Y= `SELECT @AUX`;
--enable_query_log

eval $part01 $column_list $part02 $part1_N $part2_N $part3_N ;
DROP TABLE t1;
# Bug#15407 Partitions: crash if subpartition
--error ER_PARSE_ERROR
eval $part01 $column_list $part02 $part1_N $part2_N $part3_Y ;
--error ER_PARSE_ERROR
eval $part01 $column_list $part02 $part1_N $part2_Y $part3_N ;
--error ER_PARSE_ERROR
eval $part01 $column_list $part02 $part1_N $part2_Y $part3_Y ;
--error ER_PARSE_ERROR
eval $part01 $column_list $part02 $part1_Y $part2_N $part3_N ;
--error ER_PARSE_ERROR
eval $part01 $column_list $part02 $part1_Y $part2_N $part3_Y ;
--error ER_PARSE_ERROR
eval $part01 $column_list $part02 $part1_Y $part2_Y $part3_N ;
eval $part01 $column_list $part02 $part1_Y $part2_Y $part3_Y ;
--source suite/parts/inc/partition_layout_check1.inc
DROP TABLE t1;

--echo #------------------------------------------------------------------------
--echo # 4.2 partition/subpartition numbers good and bad values and notations
--echo #------------------------------------------------------------------------
--disable_warnings
DROP TABLE IF EXISTS t1;
--enable_warnings
--echo # 4.2.1 partition/subpartition numbers INTEGER notation
# mleich: "positive/negative" is my private judgement. It need not to
#     correspond with the server response.
# (positive) number = 2
let $part_number= 2;
--source suite/parts/inc/partition_syntax_1.inc
# (positive) special case number = 1
let $part_number= 1;
--source suite/parts/inc/partition_syntax_1.inc
# (negative) 0 is non sense
let $part_number= 0;
--source suite/parts/inc/partition_syntax_1.inc
# (negative) -1 is non sense
let $part_number= -1;
--source suite/parts/inc/partition_syntax_1.inc
# (negative) 1000000 is too huge
let $part_number= 1000000;
--source suite/parts/inc/partition_syntax_1.inc

--echo # 4.2.2 partition/subpartition numbers DECIMAL notation
# (positive) number = 2.0
let $part_number= 2.0;
--source suite/parts/inc/partition_syntax_1.inc
# (negative) -2.0 is non sense
let $part_number= -2.0;
--source suite/parts/inc/partition_syntax_1.inc
# (negative) case number = 0.0 is non sense
let $part_number= 0.0;
--source suite/parts/inc/partition_syntax_1.inc
# Bug#15890 Partitions: Strange interpretation of partition number
# (negative) number = 1.6 is non sense
let $part_number= 1.6;
--source suite/parts/inc/partition_syntax_1.inc
# (negative) number is too huge
let $part_number= 999999999999999999999999999999.999999999999999999999999999999;
--source suite/parts/inc/partition_syntax_1.inc
# (negative) number is nearly zero
let $part_number= 0.000000000000000000000000000001;
--source suite/parts/inc/partition_syntax_1.inc

--echo # 4.2.3 partition/subpartition numbers FLOAT notation
##### FLOAT notation
# (positive) number = 2.0E+0
let $part_number= 2.0E+0;
--source suite/parts/inc/partition_syntax_1.inc
# Bug#15890 Partitions: Strange interpretation of partition number
# (positive) number = 0.2E+1
let $part_number= 0.2E+1;
--source suite/parts/inc/partition_syntax_1.inc
# (negative) -2.0E+0 is non sense
let $part_number= -2.0E+0;
--source suite/parts/inc/partition_syntax_1.inc
# Bug#15890 Partitions: Strange interpretation of partition number
# (negative) 0.16E+1 is non sense
let $part_number= 0.16E+1;
--source suite/parts/inc/partition_syntax_1.inc
# (negative) 0.0E+300 is zero
let $part_number= 0.0E+300;
--source suite/parts/inc/partition_syntax_1.inc
# Bug#15890 Partitions: Strange interpretation of partition number
# (negative) 1E+300 is too huge
let $part_number= 1E+300;
--source suite/parts/inc/partition_syntax_1.inc
# (negative) 1E-300 is nearly zero
let $part_number= 1E-300;
--source suite/parts/inc/partition_syntax_1.inc

--echo # 4.2.4 partition/subpartition numbers STRING notation
##### STRING notation
# (negative?) case number = '2'
let $part_number= '2';
--source suite/parts/inc/partition_syntax_1.inc
# (negative?) case number = '2.0'
let $part_number= '2.0';
--source suite/parts/inc/partition_syntax_1.inc
# (negative?) case number = '0.2E+1'
let $part_number= '0.2E+1';
--source suite/parts/inc/partition_syntax_1.inc
# (negative) Strings starts with digit, but 'A' follows
let $part_number= '2A';
--source suite/parts/inc/partition_syntax_1.inc
# (negative) Strings starts with 'A', but digit follows
let $part_number= 'A2';
--source suite/parts/inc/partition_syntax_1.inc
# (negative) empty string
let $part_number= '';
--source suite/parts/inc/partition_syntax_1.inc
# (negative) string without any digits
let $part_number= 'GARBAGE';
--source suite/parts/inc/partition_syntax_1.inc

--echo # 4.2.5 partition/subpartition numbers other notations
# (negative) Strings starts with digit, but 'A' follows
let $part_number= 2A;
--source suite/parts/inc/partition_syntax_1.inc
# (negative) Strings starts with 'A', but digit follows
let $part_number= A2;
--source suite/parts/inc/partition_syntax_1.inc
# (negative) string without any digits
let $part_number= GARBAGE;
--source suite/parts/inc/partition_syntax_1.inc

# (negative?) double quotes
let $part_number= "2";
--source suite/parts/inc/partition_syntax_1.inc
# (negative) Strings starts with digit, but 'A' follows
let $part_number= "2A";
--source suite/parts/inc/partition_syntax_1.inc
# (negative) Strings starts with 'A', but digit follows
let $part_number= "A2";
--source suite/parts/inc/partition_syntax_1.inc
# (negative) string without any digits
let $part_number= "GARBAGE";
--source suite/parts/inc/partition_syntax_1.inc

--echo # 4.2.6 (negative) partition/subpartition numbers per @variables
SET @aux = 5;
--error ER_PARSE_ERROR
eval CREATE TABLE t1 (
$column_list
)
PARTITION BY HASH(f_int1) PARTITIONS @aux;
--error ER_PARSE_ERROR
eval CREATE TABLE t1 (
$column_list
)
PARTITION BY RANGE(f_int1) SUBPARTITION BY HASH(f_int1)
SUBPARTITIONS @aux = 5
(PARTITION part1 VALUES LESS THAN ($max_row_div2),
 PARTITION part2 VALUES LESS THAN $MAX_VALUE);


--echo #------------------------------------------------------------------------
--echo # 4.3 Mixups of assigned partition/subpartition numbers and names
--echo #------------------------------------------------------------------------
--echo # 4.3.1 (positive) number of partition/subpartition
--echo #                         = number of named partition/subpartition
eval CREATE TABLE t1 (
$column_list
)
PARTITION BY HASH(f_int1) PARTITIONS 2 ( PARTITION part1, PARTITION part2 ) ;
--source suite/parts/inc/partition_layout_check1.inc
DROP TABLE t1;
eval CREATE TABLE t1 (
$column_list
)
PARTITION BY RANGE(f_int1) PARTITIONS 2
SUBPARTITION BY HASH(f_int1) SUBPARTITIONS 2
( PARTITION part1 VALUES LESS THAN (1000)
    (SUBPARTITION subpart11, SUBPARTITION subpart12),
  PARTITION part2 VALUES LESS THAN $MAX_VALUE
    (SUBPARTITION subpart21, SUBPARTITION subpart22)
);
--source suite/parts/inc/partition_layout_check1.inc
DROP TABLE t1;
--echo # 4.3.2 (positive) number of partition/subpartition ,
--echo #                  0 (= no) named partition/subpartition
--echo #                  already checked above
--echo # 4.3.3 (negative) number of partitions/subpartitions
--echo #                         > number of named partitions/subpartitions
--error ER_PARSE_ERROR
eval CREATE TABLE t1 (
$column_list
)
PARTITION BY HASH(f_int1) PARTITIONS 2 ( PARTITION part1 ) ;
# Wrong number of named subpartitions in first partition
--error ER_PARSE_ERROR
eval CREATE TABLE t1 (
$column_list
)
PARTITION BY RANGE(f_int1)
SUBPARTITION BY HASH(f_int1) SUBPARTITIONS 2
( PARTITION part1 VALUES LESS THAN (1000)
    (SUBPARTITION subpart11 ),
  PARTITION part2 VALUES LESS THAN $MAX_VALUE
    (SUBPARTITION subpart21, SUBPARTITION subpart22)
);
# Wrong number of named subpartitions in non first/non last partition
--error ER_PARSE_ERROR
eval CREATE TABLE t1 (
$column_list
)
PARTITION BY RANGE(f_int1)
SUBPARTITION BY HASH(f_int1) SUBPARTITIONS 2
( PARTITION part1 VALUES LESS THAN (1000)
    (SUBPARTITION subpart11, SUBPARTITION subpart12),
  PARTITION part2 VALUES LESS THAN (2000)
    (SUBPARTITION subpart21 ),
  PARTITION part3 VALUES LESS THAN $MAX_VALUE
    (SUBPARTITION subpart31, SUBPARTITION subpart32)
);
# Wrong number of named subpartitions in last partition
--error ER_PARSE_ERROR
eval CREATE TABLE t1 (
$column_list
)
PARTITION BY RANGE(f_int1) PARTITIONS 2
SUBPARTITION BY HASH(f_int1) SUBPARTITIONS 2
( PARTITION part1 VALUES LESS THAN (1000)
    (SUBPARTITION subpart11, SUBPARTITION subpart12),
  PARTITION part2 VALUES LESS THAN $MAX_VALUE
    (SUBPARTITION subpart21                        )
);
--echo # 4.3.4 (negative) number of partitions < number of named partitions
--error ER_PARSE_ERROR
eval CREATE TABLE t1 (
$column_list
)
PARTITION BY HASH(f_int1) PARTITIONS 1 ( PARTITION part1, PARTITION part2 ) ;
# Wrong number of named subpartitions in first partition
--error ER_PARSE_ERROR
eval CREATE TABLE t1 (
$column_list
)
PARTITION BY RANGE(f_int1)
SUBPARTITION BY HASH(f_int1) SUBPARTITIONS 1
( PARTITION part1 VALUES LESS THAN (1000)
    (SUBPARTITION subpart11, SUBPARTITION subpart12),
  PARTITION part2 VALUES LESS THAN $MAX_VALUE
    (SUBPARTITION subpart21, SUBPARTITION subpart22)
);
# Wrong number of named subpartitions in non first/non last partition
--error ER_PARSE_ERROR
eval CREATE TABLE t1 (
$column_list
)
PARTITION BY RANGE(f_int1)
SUBPARTITION BY HASH(f_int1) SUBPARTITIONS 1
( PARTITION part1 VALUES LESS THAN (1000)
    (SUBPARTITION subpart11, SUBPARTITION subpart12),
  PARTITION part2 VALUES LESS THAN (2000)
    (SUBPARTITION subpart21                        ),
  PARTITION part3 VALUES LESS THAN $MAX_VALUE
    (SUBPARTITION subpart31, SUBPARTITION subpart32)
);
# Wrong number of named subpartitions in last partition
--error ER_PARSE_ERROR
eval CREATE TABLE t1 (
$column_list
)
PARTITION BY RANGE(f_int1)
SUBPARTITION BY HASH(f_int1) SUBPARTITIONS 1
( PARTITION part1 VALUES LESS THAN (1000)
    (SUBPARTITION subpart11, SUBPARTITION subpart12),
  PARTITION part2 VALUES LESS THAN $MAX_VALUE
    (SUBPARTITION subpart21, SUBPARTITION subpart22)
);


--echo
--echo #========================================================================
--echo #  5. Checks of logical partition/subpartition name
--echo #     file name clashes during CREATE TABLE
--echo #========================================================================
--disable_warnings
DROP TABLE IF EXISTS t1;
--enable_warnings

--echo #------------------------------------------------------------------------
--echo #  5.1 (negative) A partition/subpartition name used more than once
--echo #------------------------------------------------------------------------
--echo #  5.1.1 duplicate partition name
--error ER_SAME_NAME_PARTITION
eval CREATE TABLE t1 (
$column_list
)
PARTITION BY HASH(f_int1) (PARTITION part1, PARTITION part1);
#
--echo #  5.1.2 duplicate subpartition name
# Bug#15408 Partitions: subpartition names are not unique
--error ER_SAME_NAME_PARTITION
eval CREATE TABLE t1 (
$column_list
)
PARTITION BY RANGE(f_int1)
SUBPARTITION BY HASH(f_int1)
( PARTITION part1 VALUES LESS THAN (1000)
    (SUBPARTITION subpart11, SUBPARTITION subpart11)
);

# FIXME Implement testcases with filename problems
#       existing file of other table --- partition/subpartition file name
#       partition/subpartition file name  --- file of the same table

