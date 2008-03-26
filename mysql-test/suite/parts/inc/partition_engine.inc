################################################################################
# inc/partition_engine.inc                                                     #
#                                                                              #
# Purpose:                                                                     #
#   Tests around Create/Alter partitioned tables and storage engine settings   #
#   at different places within the statement.                                  #
#   This routine is only useful for the partition_<feature>_<engine> tests.    #
#                                                                              #
# Note: There were some problems in history.                                   #
#    It looks like a table holds informations about the storage engine         #
#    for                                                                       #
#      "the whole table" -> in statement after column list before partitioning #
#      a partition       -> in statement after definition of partition         #
#      a subpartition    -> in statement after definition of subpartition      #
#    If there is a CREATE TABLE statement where not at all of these place      #
#    a storage engine is assigned, the server must decide by itself whic       #
#    storage engine to use.                                                    #
#                                                                              #
#------------------------------------------------------------------------------#
# Original Author: mleich                                                      #
# Original Date: 2006-03-05                                                    #
# Change Author:                                                               #
# Change Date:                                                                 #
# Change:                                                                      #
################################################################################

--echo
--echo #========================================================================
--echo # Checks where the engine is assigned on all supported (CREATE TABLE
--echo # statement) positions + basic operations on the tables
--echo #        Storage engine mixups are currently (2005-12-23) not supported
--echo #========================================================================
--disable_warnings
DROP TABLE IF EXISTS t1;
--enable_warnings

#
--echo #------------------------------------------------------------------------
--echo # 1 Assignment of storage engine just after column list only
--echo #------------------------------------------------------------------------
eval CREATE TABLE t1 (
$column_list
) ENGINE = $engine
     PARTITION BY HASH(f_int1) PARTITIONS 2;
INSERT INTO t1(f_int1,f_int2,f_char1,f_char2,f_charbig)
SELECT f_int1,f_int2,f_char1,f_char2,f_charbig FROM t0_template;
--source suite/parts/inc/partition_check.inc
DROP TABLE t1;
#
--echo #------------------------------------------------------------------------
--echo # 2 Assignment of storage engine just after partition or subpartition
--echo #   name only
--echo #------------------------------------------------------------------------
eval CREATE TABLE t1 (
$column_list
)
PARTITION BY HASH(f_int1)
( PARTITION part1 STORAGE ENGINE = $engine,
  PARTITION part2 STORAGE ENGINE = $engine
);
INSERT INTO t1(f_int1,f_int2,f_char1,f_char2,f_charbig)
SELECT f_int1,f_int2,f_char1,f_char2,f_charbig FROM t0_template;
--source suite/parts/inc/partition_check.inc
DROP TABLE t1;
eval CREATE TABLE t1 (
$column_list
)
PARTITION BY RANGE(f_int1)
SUBPARTITION BY HASH(f_int1)
( PARTITION part1 VALUES LESS THAN ($max_row_div2)
     (SUBPARTITION subpart11 STORAGE ENGINE = $engine,
      SUBPARTITION subpart12 STORAGE ENGINE = $engine),
  PARTITION part2 VALUES LESS THAN $MAX_VALUE
     (SUBPARTITION subpart21 STORAGE ENGINE = $engine,
      SUBPARTITION subpart22 STORAGE ENGINE = $engine)
);
INSERT INTO t1(f_int1,f_int2,f_char1,f_char2,f_charbig)
SELECT f_int1,f_int2,f_char1,f_char2,f_charbig FROM t0_template;
--source suite/parts/inc/partition_check.inc
DROP TABLE t1;
#
--echo #------------------------------------------------------------------------
--echo # 3 Some but not all named partitions or subpartitions get a storage
--echo #   engine assigned
--echo #------------------------------------------------------------------------
--error ER_MIX_HANDLER_ERROR
eval CREATE TABLE t1 (
$column_list
)
PARTITION BY HASH(f_int1)
( PARTITION part1 STORAGE ENGINE = $engine,
  PARTITION part2
);
--error ER_MIX_HANDLER_ERROR
eval CREATE TABLE t1 (
$column_list
)
PARTITION BY HASH(f_int1)
( PARTITION part1                         ,
  PARTITION part2 STORAGE ENGINE = $engine
);
--error ER_MIX_HANDLER_ERROR
eval CREATE TABLE t1 (
$column_list
)
PARTITION BY RANGE(f_int1)
SUBPARTITION BY HASH(f_int1)
( PARTITION part1 VALUES LESS THAN ($max_row_div2)
     (SUBPARTITION subpart11,
      SUBPARTITION subpart12 STORAGE ENGINE = $engine),
  PARTITION part2 VALUES LESS THAN $MAX_VALUE
     (SUBPARTITION subpart21 STORAGE ENGINE = $engine,
      SUBPARTITION subpart22 STORAGE ENGINE = $engine)
);
--error ER_MIX_HANDLER_ERROR
eval CREATE TABLE t1 (
$column_list
)
PARTITION BY RANGE(f_int1)
SUBPARTITION BY HASH(f_int1)
( PARTITION part1 VALUES LESS THAN ($max_row_div2)
     (SUBPARTITION subpart11 STORAGE ENGINE = $engine,
      SUBPARTITION subpart12 STORAGE ENGINE = $engine),
  PARTITION part2 VALUES LESS THAN $MAX_VALUE
     (SUBPARTITION subpart21,
      SUBPARTITION subpart22 )
);
eval CREATE TABLE t1 (
$column_list
)
ENGINE = $engine
PARTITION BY RANGE(f_int1)
SUBPARTITION BY HASH(f_int1)
( PARTITION part1 VALUES LESS THAN ($max_row_div2)
     (SUBPARTITION subpart11 STORAGE ENGINE = $engine,
      SUBPARTITION subpart12 STORAGE ENGINE = $engine),
  PARTITION part2 VALUES LESS THAN $MAX_VALUE
     (SUBPARTITION subpart21,
      SUBPARTITION subpart22 )
);
INSERT INTO t1(f_int1,f_int2,f_char1,f_char2,f_charbig)
SELECT f_int1,f_int2,f_char1,f_char2,f_charbig FROM t0_template;
--source suite/parts/inc/partition_check.inc
DROP TABLE t1;
#
--echo #------------------------------------------------------------------------
--echo # 4 Storage engine assignment after partition name + after name of
--echo #   subpartitions belonging to another partition
--echo #------------------------------------------------------------------------
--error ER_MIX_HANDLER_ERROR
eval CREATE TABLE t1 (
$column_list
)
PARTITION BY RANGE(f_int1)
SUBPARTITION BY HASH(f_int1)
( PARTITION part1 VALUES LESS THAN ($max_row_div2)
     (SUBPARTITION subpart11,
      SUBPARTITION subpart12),
  PARTITION part2 VALUES LESS THAN $MAX_VALUE
     (SUBPARTITION subpart21 STORAGE ENGINE = $engine,
      SUBPARTITION subpart22 STORAGE ENGINE = $engine)
);
eval CREATE TABLE t1 (
$column_list
)
ENGINE = $engine
PARTITION BY RANGE(f_int1)
SUBPARTITION BY HASH(f_int1)
( PARTITION part1 VALUES LESS THAN ($max_row_div2) ENGINE = $engine
     (SUBPARTITION subpart11,
      SUBPARTITION subpart12),
  PARTITION part2 VALUES LESS THAN $MAX_VALUE
     (SUBPARTITION subpart21,
      SUBPARTITION subpart22 STORAGE ENGINE = $engine)
);
DROP TABLE t1;
eval CREATE TABLE t1 (
$column_list
)
PARTITION BY RANGE(f_int1)
SUBPARTITION BY HASH(f_int1)
( PARTITION part1 VALUES LESS THAN ($max_row_div2) ENGINE = $engine
     (SUBPARTITION subpart11,
      SUBPARTITION subpart12),
  PARTITION part2 VALUES LESS THAN $MAX_VALUE
     (SUBPARTITION subpart21 STORAGE ENGINE = $engine,
      SUBPARTITION subpart22 STORAGE ENGINE = $engine)
);
INSERT INTO t1(f_int1,f_int2,f_char1,f_char2,f_charbig)
SELECT f_int1,f_int2,f_char1,f_char2,f_charbig FROM t0_template;
--source suite/parts/inc/partition_check.inc
DROP TABLE t1;
eval CREATE TABLE t1 (
$column_list
)
PARTITION BY RANGE(f_int1)
SUBPARTITION BY HASH(f_int1)
( PARTITION part1 VALUES LESS THAN ($max_row_div2)
     (SUBPARTITION subpart11 STORAGE ENGINE = $engine,
      SUBPARTITION subpart12 STORAGE ENGINE = $engine),
  PARTITION part2 VALUES LESS THAN $MAX_VALUE ENGINE = $engine
     (SUBPARTITION subpart21 ENGINE = $engine,
      SUBPARTITION subpart22)
);
INSERT INTO t1(f_int1,f_int2,f_char1,f_char2,f_charbig)
SELECT f_int1,f_int2,f_char1,f_char2,f_charbig FROM t0_template;
--source suite/parts/inc/partition_check.inc
DROP TABLE t1;
#
--echo #------------------------------------------------------------------------
--echo # 5 Precedence of storage engine assignments (if there is any)
--echo #------------------------------------------------------------------------
--echo # 5.1 Storage engine assignment after column list + after partition
--echo #     or subpartition name
eval CREATE TABLE t1 (
$column_list
) ENGINE = $engine
PARTITION BY HASH(f_int1)
( PARTITION part1 STORAGE ENGINE = $engine,
  PARTITION part2 STORAGE ENGINE = $engine
);
INSERT INTO t1(f_int1,f_int2,f_char1,f_char2,f_charbig)
SELECT f_int1,f_int2,f_char1,f_char2,f_charbig FROM t0_template;
--source suite/parts/inc/partition_check.inc
DROP TABLE t1;
eval CREATE TABLE t1 (
$column_list
)
PARTITION BY RANGE(f_int1)
SUBPARTITION BY HASH(f_int1)
( PARTITION part1 VALUES LESS THAN ($max_row_div2)
     (SUBPARTITION subpart11 STORAGE ENGINE = $engine,
      SUBPARTITION subpart12 STORAGE ENGINE = $engine),
  PARTITION part2 VALUES LESS THAN $MAX_VALUE
     (SUBPARTITION subpart21 STORAGE ENGINE = $engine,
      SUBPARTITION subpart22 STORAGE ENGINE = $engine)
);
INSERT INTO t1(f_int1,f_int2,f_char1,f_char2,f_charbig)
SELECT f_int1,f_int2,f_char1,f_char2,f_charbig FROM t0_template;
--source suite/parts/inc/partition_check.inc
DROP TABLE t1;
--echo # 6.2 Storage engine assignment after partition name + after
--echo #     subpartition name
--echo #     in partition part + in sub partition part
eval CREATE TABLE t1 (
$column_list
)
PARTITION BY RANGE(f_int1)
SUBPARTITION BY HASH(f_int1)
( PARTITION part1 VALUES LESS THAN ($max_row_div2) STORAGE ENGINE = $engine
     (SUBPARTITION subpart11 STORAGE ENGINE = $engine,
      SUBPARTITION subpart12 STORAGE ENGINE = $engine),
  PARTITION part2 VALUES LESS THAN $MAX_VALUE
     (SUBPARTITION subpart21 STORAGE ENGINE = $engine,
      SUBPARTITION subpart22 STORAGE ENGINE = $engine)
);
INSERT INTO t1(f_int1,f_int2,f_char1,f_char2,f_charbig)
SELECT f_int1,f_int2,f_char1,f_char2,f_charbig FROM t0_template;
--source suite/parts/inc/partition_check.inc
DROP TABLE t1;

--echo #------------------------------------------------------------------------
--echo # 6 Session default engine differs from engine used within create table
--echo #------------------------------------------------------------------------
eval SET SESSION storage_engine=$engine_other;
# Bug#16775 Partitions: strange effects on subpartitioned tables, mixed storage engines
# Bug#15966 Partitions: crash if session default engine <> engine used in create table
eval CREATE TABLE t1 (
$column_list
)
PARTITION BY HASH(f_int1) ( PARTITION part1 ENGINE = $engine);
INSERT INTO t1(f_int1,f_int2,f_char1,f_char2,f_charbig)
SELECT f_int1,f_int2,f_char1,f_char2,f_charbig FROM t0_template;
--source suite/parts/inc/partition_check.inc
DROP TABLE t1;
# Bug#15966 Partitions: crash if session default engine <> engine used in create table
eval CREATE TABLE t1 (
$column_list
)
PARTITION BY RANGE(f_int1)
SUBPARTITION BY HASH(f_int1)
( PARTITION part1 VALUES LESS THAN (1000)
     (SUBPARTITION subpart11 STORAGE ENGINE = $engine,
      SUBPARTITION subpart12 STORAGE ENGINE = $engine));
INSERT INTO t1(f_int1,f_int2,f_char1,f_char2,f_charbig)
SELECT f_int1,f_int2,f_char1,f_char2,f_charbig FROM t0_template;
--source suite/parts/inc/partition_check.inc
DROP TABLE t1;
eval SET SESSION storage_engine=$engine;
