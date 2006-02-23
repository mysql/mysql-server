-- source include/have_partition.inc

# include/partition_1.inc
#
# Partitionong tests
#
# Attention: The variable
#                 $engine            -- Storage engine to be tested.
#            must be set within the script sourcing this file.
#
--disable_abort_on_error
SET AUTOCOMMIT= 1;

##### Disabled testcases, because of open bugs #####
--echo
--echo #------------------------------------------------------------------------
--echo # There are several testcases disabled because ouf the open bugs 
--echo # #15407 , #15408 , #15890 , #15961 , #13447 , #15966 , #15968, #16370
--echo #------------------------------------------------------------------------
# Bug#15407 Partitions: crash if subpartition
let $fixed_bug15407= 0;
# Bug#15408 Partitions: subpartition names are not unique
let $fixed_bug15408= 0;
# Bug#15890 Partitions: Strange interpretation of partition number
let $fixed_bug15890= 0;
# Bug#15961 Partitions: Creation of subpart. table without subpart. rule not rejected
let $fixed_bug15961= 0;
# Bug#13447 Partitions: crash with alter table
let $fixed_bug13447= 0;
# Bug#15966 Partitions: crash if session default engine <> engine used in create table
let $fixed_bug15966= 0;
# Bug#15968 Partitions: crash when INSERT with f1 = -1 into PARTITION BY HASH(f1)
let $fixed_bug15968= 0;
# Bug #16370  	Partitions: subpartitions names not mentioned in SHOW CREATE TABLE output
let $fixed_bug16370= 0;

##### Option, for displaying files #####
#
# Attention: Displaying the directory content via "ls $MYSQLTEST_VARDIR/master-data/test/t*"
#            is probably not portable.
#            let $ls= 0; disables the execution of "ls ....."
let $ls= 0;

################################################################################
#                     Partitioning syntax
#
# CREATE TABLE .... (column-list ..)
# PARTITION BY
#     KEY '(' ( column-list ) ')'
#   | RANGE '(' ( expr ) ')'
#   | LIST '(' ( expr ) ')'
#   | HASH '(' ( expr ) ')'
# [PARTITIONS num ]
# [SUBPARTITION BY
#   KEY '(' ( column-list ) ')'
#   | HASH '(' ( expr ) ')'
#   [SUBPARTITIONS num ]
# ]
# [ '('
#      ( PARTITION logical-name
#       [ VALUES LESS THAN '(' ( expr | MAX_VALUE ) ')' ]
#       [ VALUES IN '(' (expr)+ ')' ]
#       [ TABLESPACE tablespace-name ]
#       [ [ STORAGE ] ENGINE [ '=' ] storage-engine-name ]
#       [ NODEGROUP nodegroup-id ]
#       [ '('
#            ( SUBPARTITION logical-name
#              [ TABLESPACE tablespace-name ]
#              [ STORAGE ENGINE = storage-engine-name ]
#              [ NODEGROUP nodegroup-id ]
#            )+
#         ')'
#      )+
#   ')'
# ]
################################################################################

--echo
--echo #------------------------------------------------------------------------
--echo #  0. Setting of auxiliary variables + Creation of an auxiliary table
--echo #     needed in all testcases
--echo #------------------------------------------------------------------------
let $max_row= `SELECT @max_row`;
let $max_row_div2= `SELECT @max_row DIV 2`;
let $max_row_div3= `SELECT @max_row DIV 3`;
let $max_row_div4= `SELECT @max_row DIV 4`;
let $max_int_4= 2147483647;
--disable_warnings
DROP TABLE IF EXISTS t0_template;
--enable_warnings
CREATE TABLE t0_template ( f1 INTEGER, f2 char(20), PRIMARY KEY(f1))
ENGINE = MEMORY;
--echo #     Logging of <max_row> INSERTs into t0_template suppressed
--disable_query_log
let $num= $max_row;
while ($num)
{
  eval INSERT INTO t0_template SET f1 = $num, f2 = '---$num---';

  dec $num;
}
--enable_query_log

--echo
--echo #------------------------------------------------------------------------
--echo #  1. Some syntax checks
--echo #------------------------------------------------------------------------
--echo #  1.1 Subpartioned table without subpartitioning rule must be rejected
--disable_warnings
DROP TABLE IF EXISTS t1;
--enable_warnings
if ($fixed_bug15961)
{
# Bug#15961 Partitions: Creation of subpart. table without subpart. rule not rejected
--error 9999
CREATE TABLE t1 ( f1 INTEGER, f2 char(20))
PARTITION BY RANGE(f1)
( PARTITION part1 VALUES LESS THAN (1000) (SUBPARTITION subpart11));
}
--echo #  FIXME Implement testcases, where it is checked that all create and
--echo #  alter table statements
--echo #  - with missing mandatory parameters are rejected
--echo #  - with optional parameters are accepted
--echo #  - with wrong combinations of optional parameters are rejected
--echo #  - ............

--echo
--echo #------------------------------------------------------------------------
--echo #  2. Checks where the engine is assigned on all supported (CREATE TABLE
--echo #     statement) positions + basic operations on the tables
--echo #        Storage engine mixups are currently (2005-12-23) not supported
--echo #------------------------------------------------------------------------
--disable_warnings
DROP TABLE IF EXISTS t1;
--enable_warnings

--echo # 2.1 non partitioned table (for comparison)
eval CREATE TABLE t1 ( f1 INTEGER, f2 char(20)) ENGINE = $engine;
# MLML Full size (as check of check routine)
--source include/partition_10.inc
DROP TABLE t1;
#
--echo # 2.2 Assignment of storage engine just after column list only
eval CREATE TABLE t1 ( f1 INTEGER, f2 char(20)) ENGINE = $engine
     PARTITION BY HASH(f1) PARTITIONS 2;
--source include/partition_10.inc
DROP TABLE t1;
#
--echo # 2.3 Assignment of storage engine just after partition or subpartition
--echo #     name only
eval CREATE TABLE t1 ( f1 INTEGER, f2 char(20))
     PARTITION BY HASH(f1)
     ( PARTITION part1 STORAGE ENGINE = $engine,
       PARTITION part2 STORAGE ENGINE = $engine
     );
--source include/partition_10.inc
DROP TABLE t1;
eval CREATE TABLE t1 ( f1 INTEGER, f2 char(20))
     PARTITION BY RANGE(f1)
     SUBPARTITION BY HASH(f1)
     ( PARTITION part1 VALUES LESS THAN ($max_row_div2)
          (SUBPARTITION subpart11 STORAGE ENGINE = $engine,
           SUBPARTITION subpart12 STORAGE ENGINE = $engine),
       PARTITION part2 VALUES LESS THAN ($max_int_4)
          (SUBPARTITION subpart21 STORAGE ENGINE = $engine,
           SUBPARTITION subpart22 STORAGE ENGINE = $engine)
     );
--source include/partition_10.inc
DROP TABLE t1;
#
--echo # 2.4 Some but not all named partitions or subpartitions get a storage
--echo #     engine assigned
eval CREATE TABLE t1 ( f1 INTEGER, f2 char(20))
     PARTITION BY HASH(f1)
     ( PARTITION part1 STORAGE ENGINE = $engine,
       PARTITION part2
     );
--source include/partition_10.inc
DROP TABLE t1;
eval CREATE TABLE t1 ( f1 INTEGER, f2 char(20))
     PARTITION BY HASH(f1)
     ( PARTITION part1                         ,
       PARTITION part2 STORAGE ENGINE = $engine
     );
--source include/partition_10.inc
DROP TABLE t1;
eval CREATE TABLE t1 ( f1 INTEGER, f2 char(20))
     PARTITION BY RANGE(f1)
     SUBPARTITION BY HASH(f1)
     ( PARTITION part1 VALUES LESS THAN ($max_row_div2)
          (SUBPARTITION subpart11,
           SUBPARTITION subpart12 STORAGE ENGINE = $engine),
       PARTITION part2 VALUES LESS THAN ($max_int_4)
          (SUBPARTITION subpart21 STORAGE ENGINE = $engine,
           SUBPARTITION subpart22 STORAGE ENGINE = $engine)
     );
--source include/partition_10.inc
DROP TABLE t1;
eval CREATE TABLE t1 ( f1 INTEGER, f2 char(20))
     PARTITION BY RANGE(f1)
     SUBPARTITION BY HASH(f1)
     ( PARTITION part1 VALUES LESS THAN ($max_row_div2)
          (SUBPARTITION subpart11 STORAGE ENGINE = $engine,
           SUBPARTITION subpart12 STORAGE ENGINE = $engine),
       PARTITION part2 VALUES LESS THAN ($max_int_4)
          (SUBPARTITION subpart21,
           SUBPARTITION subpart22 )
     );
--source include/partition_10.inc
DROP TABLE t1;
#
--echo # 2.5 Storage engine assignment after partition name + after name of
--echo #     subpartitions belonging to another partition
eval CREATE TABLE t1 ( f1 INTEGER, f2 char(20))
     PARTITION BY RANGE(f1)
     SUBPARTITION BY HASH(f1)
     ( PARTITION part1 VALUES LESS THAN ($max_row_div2) ENGINE = $engine
          (SUBPARTITION subpart11,
           SUBPARTITION subpart12),
       PARTITION part2 VALUES LESS THAN ($max_int_4)
          (SUBPARTITION subpart21 STORAGE ENGINE = $engine,
           SUBPARTITION subpart22 STORAGE ENGINE = $engine)
     );
--source include/partition_10.inc
DROP TABLE t1;
eval CREATE TABLE t1 ( f1 INTEGER, f2 char(20))
     PARTITION BY RANGE(f1)
     SUBPARTITION BY HASH(f1)
     ( PARTITION part1 VALUES LESS THAN ($max_row_div2)
          (SUBPARTITION subpart11 STORAGE ENGINE = $engine,
           SUBPARTITION subpart12 STORAGE ENGINE = $engine),
       PARTITION part2 VALUES LESS THAN ($max_int_4) ENGINE = $engine
          (SUBPARTITION subpart21,
           SUBPARTITION subpart22)
     );
--source include/partition_10.inc
DROP TABLE t1;
#
--echo # 2.6 Precedence of storage engine assignments
--echo # 2.6.1 Storage engine assignment after column list + after partition
--echo #       or subpartition name
eval CREATE TABLE t1 ( f1 INTEGER, f2 char(20)) ENGINE = $engine
     PARTITION BY HASH(f1)
     ( PARTITION part1 STORAGE ENGINE = $engine,
       PARTITION part2 STORAGE ENGINE = $engine
     );
--source include/partition_10.inc
DROP TABLE t1;
eval CREATE TABLE t1 ( f1 INTEGER, f2 char(20)) ENGINE = $engine
     PARTITION BY RANGE(f1)
     SUBPARTITION BY HASH(f1)
     ( PARTITION part1 VALUES LESS THAN ($max_row_div2)
          (SUBPARTITION subpart11 STORAGE ENGINE = $engine,
           SUBPARTITION subpart12 STORAGE ENGINE = $engine),
       PARTITION part2 VALUES LESS THAN ($max_int_4)
          (SUBPARTITION subpart21 STORAGE ENGINE = $engine,
           SUBPARTITION subpart22 STORAGE ENGINE = $engine)
     );
--source include/partition_10.inc
DROP TABLE t1;
--echo # 2.6.2 Storage engine assignment after partition name + after 
--echo #       subpartition name
#     in partition part + in sub partition part
eval CREATE TABLE t1 ( f1 INTEGER, f2 char(20))
     PARTITION BY RANGE(f1)
     SUBPARTITION BY HASH(f1)
     ( PARTITION part1 VALUES LESS THAN ($max_row_div2) STORAGE ENGINE = $engine
          (SUBPARTITION subpart11 STORAGE ENGINE = $engine,
           SUBPARTITION subpart12 STORAGE ENGINE = $engine),
       PARTITION part2 VALUES LESS THAN ($max_int_4)
          (SUBPARTITION subpart21 STORAGE ENGINE = $engine,
           SUBPARTITION subpart22 STORAGE ENGINE = $engine)
     );
--source include/partition_10.inc
DROP TABLE t1;

--echo # 2.7 Session default engine differs from engine used within create table
eval SET SESSION storage_engine=$engine_other;
if ($fixed_bug15966)
{
# Bug#15966 Partitions: crash if session default engine <> engine used in create table
eval CREATE TABLE t1 ( f1 INTEGER, f2 char(20))
PARTITION BY HASH(f1) ( PARTITION part1 ENGINE = $engine);
--source include/partition_10.inc
DROP TABLE t1;
# Bug#15966 Partitions: crash if session default engine <> engine used in create table
eval CREATE TABLE t1 ( f1 INTEGER, f2 char(20))
     PARTITION BY RANGE(f1)
     SUBPARTITION BY HASH(f1)
     ( PARTITION part1 VALUES LESS THAN (1000)
          (SUBPARTITION subpart11 STORAGE ENGINE = $engine,
           SUBPARTITION subpart12 STORAGE ENGINE = $engine));
--source include/partition_10.inc
DROP TABLE t1;
}
eval SET SESSION storage_engine=$engine;


--echo
--echo #------------------------------------------------------------------------
--echo #  3. Check assigning the number of partitions and subpartitions
--echo #     with and without named partitions/subpartitions
--echo #------------------------------------------------------------------------
--disable_warnings
DROP TABLE IF EXISTS t1;
--enable_warnings
--echo # 3.1 (positive) without partition/subpartition number assignment
--echo # 3.1.1 no partition number, no named partitions
CREATE TABLE t1 ( f1 INTEGER, f2 char(20))
PARTITION BY HASH(f1);
--source include/partition_10.inc
DROP TABLE t1;
--echo # 3.1.2 no partition number, named partitions
CREATE TABLE t1 ( f1 INTEGER, f2 char(20))
PARTITION BY HASH(f1) (PARTITION part1, PARTITION part2);
--source include/partition_10.inc
DROP TABLE t1;
# Attention: Several combinations are impossible
#           If subpartitioning exists
#           - partitioning algorithm must be RANGE or LIST
#             This implies the assignment of named partitions.
#           - subpartitioning algorithm must be HASH or KEY
--echo # 3.1.3 variations on no partition/subpartition number, named partitions,
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
let $part0= CREATE TABLE t1 ( f1 INTEGER, f2 char(20))
PARTITION BY RANGE(f1) SUBPARTITION BY HASH(f1);
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
eval SET @aux = 'PARTITION part3 VALUES LESS THAN ($max_int_4))';
let $part3_N= `SELECT @AUX`;
eval SET @aux = 'PARTITION part3 VALUES LESS THAN ($max_int_4)
(SUBPARTITION subpart31 , SUBPARTITION subpart32 ))';
let $part3_Y= `SELECT @AUX`;
--enable_query_log

eval $part0 $part1_N $part2_N $part3_N ;
DROP TABLE t1;
# Bug#15407 Partitions: crash if subpartition
if ($fixed_bug15407)
{
eval $part0 $part1_N $part2_N $part3_Y ;
--source include/partition_10.inc
DROP TABLE t1;
eval $part0 $part1_N $part2_Y $part3_N ;
--source include/partition_10.inc
DROP TABLE t1;
eval $part0 $part1_N $part2_Y $part3_Y ;
--source include/partition_10.inc
DROP TABLE t1;
eval $part0 $part1_Y $part2_N $part3_N ;
--source include/partition_10.inc
DROP TABLE t1;
eval $part0 $part1_Y $part2_N $part3_Y ;
--source include/partition_10.inc
DROP TABLE t1;
eval $part0 $part1_Y $part2_Y $part3_N ;
--source include/partition_10.inc
DROP TABLE t1;
}
eval $part0 $part1_Y $part2_Y $part3_Y ;
--source include/partition_10.inc
DROP TABLE t1;

--echo # 3.2 partition/subpartition numbers good and bad values and notations
--disable_warnings
DROP TABLE IF EXISTS t1;
--enable_warnings
--echo # 3.2.1 partition/subpartition numbers INTEGER notation
# ML: "positive/negative" is my private judgement. It need no to correspond
#     with the server response.
# (positive) number = 2
let $part_number= 2;
--source include/partition_11.inc
# (positive) special case number = 1
let $part_number= 1;
--source include/partition_11.inc
# (negative) 0 is non sense
let $part_number= 0;
--source include/partition_11.inc
# (negative) -1 is non sense
let $part_number= -1;
--source include/partition_11.inc
# (negative) 1000000 is too huge
let $part_number= 1000000;
--source include/partition_11.inc

if ($fixed_bug15890)
{
--echo # 3.2.2 partition/subpartition numbers DECIMAL notation
# (positive) number = 2.0
let $part_number= 2.0;
--source include/partition_11.inc
# (negative) -2.0 is non sense
let $part_number= -2.0;
--source include/partition_11.inc
# (negative) case number = 0.0 is non sense
let $part_number= 0.0;
--source include/partition_11.inc
# Bug#15890 Partitions: Strange interpretation of partition number
# (negative) number = 1.5 is non sense
let $part_number= 1.5;
--source include/partition_11.inc
# (negative) number is too huge
let $part_number= 999999999999999999999999999999.999999999999999999999999999999;
--source include/partition_11.inc
# (negative) number is nearly zero
let $part_number= 0.000000000000000000000000000001;
--source include/partition_11.inc

--echo # 3.2.3 partition/subpartition numbers FLOAT notation
##### FLOAT notation
# (positive) number = 2.0E+0
let $part_number= 2.0E+0;
--source include/partition_11.inc
# Bug#15890 Partitions: Strange interpretation of partition number
# (positive) number = 0.2E+1
let $part_number= 0.2E+1;
--source include/partition_11.inc
# (negative) -2.0E+0 is non sense
let $part_number= -2.0E+0;
--source include/partition_11.inc
# (negative) 0.15E+1 is non sense
let $part_number= 0.15E+1;
--source include/partition_11.inc
# (negative) 0.0E+300 is zero
let $part_number= 0.0E+300;
--source include/partition_11.inc
# Bug#15890 Partitions: Strange interpretation of partition number
# (negative) 1E+300 is too huge
let $part_number= 1E+300;
--source include/partition_11.inc
# (negative) 1E-300 is nearly zero
let $part_number= 1E-300;
--source include/partition_11.inc
}

--echo # 3.2.4 partition/subpartition numbers STRING notation
##### STRING notation
# (negative?) case number = '2'
let $part_number= '2';
--source include/partition_11.inc
# (negative?) case number = '2.0'
let $part_number= '2.0';
--source include/partition_11.inc
# (negative?) case number = '0.2E+1'
let $part_number= '0.2E+1';
--source include/partition_11.inc
# (negative) Strings starts with digit, but 'A' follows
let $part_number= '2A';
--source include/partition_11.inc
# (negative) Strings starts with 'A', but digit follows
let $part_number= 'A2';
--source include/partition_11.inc
# (negative) empty string
let $part_number= '';
--source include/partition_11.inc
# (negative) string without any digits
let $part_number= 'GARBAGE';
--source include/partition_11.inc

--echo # 3.2.5 partition/subpartition numbers other notations
# (negative) Strings starts with digit, but 'A' follows
let $part_number= 2A;
--source include/partition_11.inc
# (negative) Strings starts with 'A', but digit follows
let $part_number= A2;
--source include/partition_11.inc
# (negative) string without any digits
let $part_number= GARBAGE;
--source include/partition_11.inc

# (negative?) double quotes
let $part_number= "2";
--source include/partition_11.inc
# (negative) Strings starts with digit, but 'A' follows
let $part_number= "2A";
--source include/partition_11.inc
# (negative) Strings starts with 'A', but digit follows
let $part_number= "A2";
--source include/partition_11.inc
# (negative) string without any digits
let $part_number= "GARBAGE";
--source include/partition_11.inc

--echo # 3.3 Mixups of assigned partition/subpartition numbers and names
--echo # 3.3.1 (positive) number of partition/subpartition
--echo #                         = number of named partition/subpartition
CREATE TABLE t1 ( f1 INTEGER, f2 char(20))
PARTITION BY HASH(f1) PARTITIONS 2 ( PARTITION part1, PARTITION part2 ) ;
SHOW CREATE TABLE t1;
DROP TABLE t1;
CREATE TABLE t1 ( f1 INTEGER, f2 char(20))
PARTITION BY RANGE(f1) PARTITIONS 2
SUBPARTITION BY HASH(f1) SUBPARTITIONS 2
( PARTITION part1 VALUES LESS THAN (1000)
    (SUBPARTITION subpart11, SUBPARTITION subpart12),
  PARTITION part2 VALUES LESS THAN (2147483647)
    (SUBPARTITION subpart21, SUBPARTITION subpart22)
);
--source include/partition_layout.inc
DROP TABLE t1;
--echo # 3.3.2 (positive) number of partition/subpartition ,
--echo #                  0 (= no) named partition/subpartition
--echo #                  already checked above
--echo # 3.3.3 (negative) number of partitions/subpartitions
--echo #                         > number of named partitions/subpartitions
--error 1064
CREATE TABLE t1 ( f1 INTEGER, f2 char(20))
PARTITION BY HASH(f1) PARTITIONS 2 ( PARTITION part1 ) ;
# Wrong number of named subpartitions in first partition
--error 1064
CREATE TABLE t1 ( f1 INTEGER, f2 char(20))
PARTITION BY RANGE(f1)
SUBPARTITION BY HASH(f1) SUBPARTITIONS 2
( PARTITION part1 VALUES LESS THAN (1000)
    (SUBPARTITION subpart11 ),
  PARTITION part2 VALUES LESS THAN (2147483647)
    (SUBPARTITION subpart21, SUBPARTITION subpart22)
);
# Wrong number of named subpartitions in non first/non last partition
--error 1064
CREATE TABLE t1 ( f1 INTEGER, f2 char(20))
PARTITION BY RANGE(f1)
SUBPARTITION BY HASH(f1) SUBPARTITIONS 2
( PARTITION part1 VALUES LESS THAN (1000)
    (SUBPARTITION subpart11, SUBPARTITION subpart12),
  PARTITION part2 VALUES LESS THAN (2000)
    (SUBPARTITION subpart21 ),
  PARTITION part3 VALUES LESS THAN (2147483647)
    (SUBPARTITION subpart31, SUBPARTITION subpart32)
);
# Wrong number of named subpartitions in last partition
--error 1064
CREATE TABLE t1 ( f1 INTEGER, f2 char(20))
PARTITION BY RANGE(f1) PARTITIONS 2
SUBPARTITION BY HASH(f1) SUBPARTITIONS 2
( PARTITION part1 VALUES LESS THAN (1000)
    (SUBPARTITION subpart11, SUBPARTITION subpart12),
  PARTITION part2 VALUES LESS THAN (2147483647)
    (SUBPARTITION subpart21                        )
);
--echo # 3.3.4 (negative) number of partitions < number of named partitions
--error 1064
CREATE TABLE t1 ( f1 INTEGER, f2 char(20))
PARTITION BY HASH(f1) PARTITIONS 1 ( PARTITION part1, PARTITION part2 ) ;
# Wrong number of named subpartitions in first partition
--error 1064
CREATE TABLE t1 ( f1 INTEGER, f2 char(20))
PARTITION BY RANGE(f1)
SUBPARTITION BY HASH(f1) SUBPARTITIONS 1
( PARTITION part1 VALUES LESS THAN (1000)
    (SUBPARTITION subpart11, SUBPARTITION subpart12),
  PARTITION part2 VALUES LESS THAN (2147483647)
    (SUBPARTITION subpart21, SUBPARTITION subpart22)
);
# Wrong number of named subpartitions in non first/non last partition
--error 1064
CREATE TABLE t1 ( f1 INTEGER, f2 char(20))
PARTITION BY RANGE(f1)
SUBPARTITION BY HASH(f1) SUBPARTITIONS 1
( PARTITION part1 VALUES LESS THAN (1000)
    (SUBPARTITION subpart11, SUBPARTITION subpart12),
  PARTITION part2 VALUES LESS THAN (2000)
    (SUBPARTITION subpart21                        ),
  PARTITION part3 VALUES LESS THAN (2147483647)
    (SUBPARTITION subpart31, SUBPARTITION subpart32)
);
# Wrong number of named subpartitions in last partition
--error 1064
CREATE TABLE t1 ( f1 INTEGER, f2 char(20))
PARTITION BY RANGE(f1)
SUBPARTITION BY HASH(f1) SUBPARTITIONS 1
( PARTITION part1 VALUES LESS THAN (1000)
    (SUBPARTITION subpart11, SUBPARTITION subpart12),
  PARTITION part2 VALUES LESS THAN (2147483647)
    (SUBPARTITION subpart21, SUBPARTITION subpart22)
);


--echo
--echo #------------------------------------------------------------------------
--echo #  4. Checks of logical partition/subpartition name
--echo #     file name clashes during CREATE TABLE
--echo #------------------------------------------------------------------------
--disable_warnings
DROP TABLE IF EXISTS t1;
--enable_warnings

--echo #  4.1 (negative) A partition name used more than once
--error ER_SAME_NAME_PARTITION
CREATE TABLE t1 ( f1 INTEGER, f2 char(20))
PARTITION BY HASH(f1) (PARTITION part1, PARTITION part1);
#
if ($fixed_bug15408)
{
# Bug#15408 Partitions: subpartition names are not unique
--error ER_SAME_NAME_PARTITION
CREATE TABLE t1 ( f1 INTEGER, f2 char(20))
PARTITION BY RANGE(f1)
SUBPARTITION BY HASH(f1)
( PARTITION part1 VALUES LESS THAN (1000)
    (SUBPARTITION subpart11, SUBPARTITION subpart11)
);
}
--echo # FIXME Implement testcases with filename problems
--echo #       existing file of other table --- partition/subpartition file name
--echo #       partition/subpartition file name  --- file of the same table

--echo
--echo #------------------------------------------------------------------------
--echo #  5. Alter table experiments
--echo #------------------------------------------------------------------------
--disable_warnings
DROP TABLE IF EXISTS t1;
--enable_warnings
--echo # 5.1 alter table add partition
--echo # 5.1.1 (negative) add partition to non partitioned table
CREATE TABLE t1 ( f1 INTEGER, f2 char(20));
--source include/partition_layout.inc
# MyISAM gets ER_PARTITION_MGMT_ON_NONPARTITIONED and NDB 1005
# The error code of NDB differs, because all NDB tables are partitioned even
# if the CREATE TABLE does not contain a partitioning clause.
--error ER_PARTITION_MGMT_ON_NONPARTITIONED,1005
ALTER TABLE t1 ADD PARTITION (PARTITION part1);
--source include/partition_layout.inc
DROP TABLE t1;

--echo # 5.1.2 Add one partition to a table with one partition
CREATE TABLE t1 ( f1 INTEGER, f2 char(20))
PARTITION BY HASH(f1);
--source include/partition_layout.inc
eval INSERT INTO t1 SELECT * FROM t0_template WHERE f1 BETWEEN 1 AND $max_row_div2 - 1;
--disable_query_log
eval SELECT $engine = 'NDB' INTO @aux;
let $my_exit= `SELECT @aux`;
if ($my_exit)
{
   exit;
}
--enable_query_log
ALTER TABLE t1 ADD PARTITION (PARTITION part1);
--source include/partition_12.inc
DROP TABLE t1;

--echo # 5.1.3 Several times add one partition to a table with some partitions
CREATE TABLE t1 ( f1 INTEGER, f2 char(20))
PARTITION BY HASH(f1) (PARTITION part1, PARTITION part3);
--source include/partition_layout.inc
eval INSERT INTO t1 SELECT * FROM t0_template WHERE f1 BETWEEN 1 AND $max_row_div2 - 1;
# Partition name before first existing partition name
ALTER TABLE t1 ADD PARTITION (PARTITION part0);
--source include/partition_12.inc
DELETE FROM t1;
eval INSERT INTO t1 SELECT * FROM t0_template WHERE f1 BETWEEN 1 AND $max_row_div2 - 1;
# Partition name between existing partition names
ALTER TABLE t1 ADD PARTITION (PARTITION part2);
--source include/partition_12.inc
DELETE FROM t1;
eval INSERT INTO t1 SELECT * FROM t0_template WHERE f1 BETWEEN 1 AND $max_row_div2 - 1;
if ($fixed_bug13447)
{
# Partition name after all existing partition names
# Bug#13447 Partitions: crash with alter table
ALTER TABLE t1 ADD PARTITION (PARTITION part4);
}
--source include/partition_12.inc
DROP TABLE t1;

--echo # 5.1.4 Add several partitions to a table with some partitions
CREATE TABLE t1 ( f1 INTEGER, f2 char(20))
PARTITION BY HASH(f1) (PARTITION part1, PARTITION part3);
--source include/partition_layout.inc
eval INSERT INTO t1 SELECT * FROM t0_template WHERE f1 BETWEEN 1 AND $max_row_div2 - 1;
if ($fixed_bug13447)
{
# Bug#13447 Partitions: crash with alter table
ALTER TABLE t1 ADD PARTITION (PARTITION part0, PARTITION part2, PARTITION part4);
}
--source include/partition_12.inc
DROP TABLE t1;

--echo # 5.1.5 (negative) Add partitions to a table with some partitions
--echo #       clash on new and already existing partition names
CREATE TABLE t1 ( f1 INTEGER, f2 char(20))
PARTITION BY HASH(f1) (PARTITION part1, PARTITION part2, PARTITION part3);
# Clash on first/non last partition name
--error ER_SAME_NAME_PARTITION
ALTER TABLE t1 ADD PARTITION (PARTITION part1);
# Clash on non first/non last partition name
--error ER_SAME_NAME_PARTITION
ALTER TABLE t1 ADD PARTITION (PARTITION part2);
# Clash on non first/last partition name
--error ER_SAME_NAME_PARTITION
ALTER TABLE t1 ADD PARTITION (PARTITION part3);
# Clash on all partition names
--error ER_SAME_NAME_PARTITION
ALTER TABLE t1 ADD PARTITION (PARTITION part1, PARTITION part2, PARTITION part3);
DROP TABLE t1;

# FIXME Is there any way to add a subpartition to an already existing partition

--echo # 5.2 alter table add subpartition
--echo # 5.2.1 Add one subpartition to a table with subpartitioning rule and
--echo #       no explicit defined subpartitions
eval CREATE TABLE t1 ( f1 INTEGER, f2 char(20))
PARTITION BY RANGE(f1)
SUBPARTITION BY HASH(f1)
(PARTITION part1 VALUES LESS THAN ($max_row_div2));
if ($fixed_bug16370)
{
--source include/partition_layout.inc
}
eval INSERT INTO t1 SELECT * FROM t0_template WHERE f1 BETWEEN 1 AND $max_row_div2 - 1;
eval ALTER TABLE t1 ADD PARTITION (PARTITION part2 VALUES LESS THAN ($max_int_4)
    (SUBPARTITION subpart21));
if ($fixed_bug16370)
{
--source include/partition_12.inc
}
DROP TABLE t1;
DROP TABLE if exists t0_template;
