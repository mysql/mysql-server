################################################################################
# inc/partition_value.inc                                                      #
#                                                                              #
# Purpose:                                                                     #
#   Tests around "exotic" values calculated by the partitioning function       #
#                                                                              #
#------------------------------------------------------------------------------#
# Original Author: mleich                                                      #
# Original Date: 2006-04-11                                                    #
# Change Author:                                                               #
# Change Date:                                                                 #
# Change:                                                                      #
################################################################################


--echo
--echo This test relies on the CAST() function for partitioning, which
--echo is not allowed.  Not deleting it yet, as it may have some useful
--echo bits in it.  See Bug #30581, "partition_value tests use disallowed
--echo CAST() function"
--echo

--disable_testcase BUG#30581

--echo
--echo #========================================================================
--echo #  Calculation of "exotic" results within the partition function
--echo #        outside of SIGNED BIGINT value range, 0, NULL
--echo #     column used in partitioning function has type CHAR
--echo #========================================================================
--echo #  1.   HASH(<check value>)
--disable_warnings
DROP TABLE IF EXISTS t1;
--enable_warnings
#
eval CREATE TABLE t1 (
$column_list
)
PARTITION BY HASH(CAST(f_char1 AS SIGNED INTEGER) * CAST(5.0E+18 AS SIGNED INTEGER)) PARTITIONS 8;
let $my_val= 2147483646;
eval INSERT INTO t1(f_int1,f_int2,f_char1,f_char2,f_charbig)
VALUES($my_val,$my_val,'$my_val','$my_val','#$my_val#');
eval SELECT COUNT(*) = 1 FROM t1 WHERE f_char1 = '$my_val';
let $my_val= -2147483646;
eval INSERT INTO t1(f_int1,f_int2,f_char1,f_char2,f_charbig)
VALUES($my_val,$my_val,'$my_val','$my_val','#$my_val#');
eval SELECT COUNT(*) = 1 FROM t1 WHERE f_char1 = '$my_val';
let $my_val= 0;
eval INSERT INTO t1(f_int1,f_int2,f_char1,f_char2,f_charbig)
VALUES($my_val,$my_val,'$my_val','$my_val','#$my_val#');
eval SELECT COUNT(*) = 1 FROM t1 WHERE f_char1 = '$my_val';
# let $my_val= NULL;
eval INSERT INTO t1(f_int1,f_int2,f_char1,f_char2,f_charbig)
VALUES(NULL,NULL,NULL,NULL,NULL);
eval SELECT COUNT(*) = 1 FROM t1 WHERE f_char1 IS NULL;
DROP TABLE t1;
#
--echo #  2.   RANGE(<check value>)
eval CREATE TABLE t1 (
$column_list
)
PARTITION BY RANGE(CAST(f_char1 AS SIGNED INTEGER) * CAST(5.0E+18 AS SIGNED INTEGER))
(PARTITION p0 VALUES LESS THAN (0),
 PARTITION p1 VALUES LESS THAN (1000000),
 PARTITION p2 VALUES LESS THAN MAXVALUE);
let $my_val= 2147483646;
eval INSERT INTO t1(f_int1,f_int2,f_char1,f_char2,f_charbig)
VALUES($my_val,$my_val,'$my_val','$my_val','#$my_val#');
eval SELECT COUNT(*) = 1 FROM t1 WHERE f_char1 = '$my_val';
let $my_val= -2147483646;
eval INSERT INTO t1(f_int1,f_int2,f_char1,f_char2,f_charbig)
VALUES($my_val,$my_val,'$my_val','$my_val','#$my_val#');
eval SELECT COUNT(*) = 1 FROM t1 WHERE f_char1 = '$my_val';
let $my_val= 0;
eval INSERT INTO t1(f_int1,f_int2,f_char1,f_char2,f_charbig)
VALUES($my_val,$my_val,'$my_val','$my_val','#$my_val#');
eval SELECT COUNT(*) = 1 FROM t1 WHERE f_char1 = '$my_val';
# let $my_val= NULL;
eval INSERT INTO t1(f_int1,f_int2,f_char1,f_char2,f_charbig)
VALUES(NULL,NULL,NULL,NULL,NULL);
eval SELECT COUNT(*) = 1 FROM t1 WHERE f_char1 IS NULL;
DROP TABLE t1;
#
# The NDB handler only supports 32 bit integers in VALUES
# therefor we have to skip the next test for NDB.
if (`SELECT @@session.default_storage_engine NOT IN('ndbcluster')`)
{
--echo #  3.   LIST(<check value>)
eval CREATE TABLE t1 (
$column_list
)
PARTITION BY LIST(CAST(f_char1 AS SIGNED INTEGER) * CAST(5.0E+18 AS SIGNED INTEGER))
(PARTITION p0 VALUES IN (0),
 PARTITION p1 VALUES IN (NULL),
 PARTITION p2 VALUES IN (CAST( 2147483646 AS SIGNED INTEGER) * CAST(5.0E+18 AS SIGNED INTEGER)),
 PARTITION p3 VALUES IN (CAST(-2147483646 AS SIGNED INTEGER) * CAST(5.0E+18 AS SIGNED INTEGER)));
let $my_val= 2147483646;
eval INSERT INTO t1(f_int1,f_int2,f_char1,f_char2,f_charbig)
VALUES($my_val,$my_val,'$my_val','$my_val','#$my_val#');
eval SELECT COUNT(*) = 1 FROM t1 WHERE f_char1 = '$my_val';
let $my_val= -2147483646;
eval INSERT INTO t1(f_int1,f_int2,f_char1,f_char2,f_charbig)
VALUES($my_val,$my_val,'$my_val','$my_val','#$my_val#');
eval SELECT COUNT(*) = 1 FROM t1 WHERE f_char1 = '$my_val';
let $my_val= 0;
eval INSERT INTO t1(f_int1,f_int2,f_char1,f_char2,f_charbig)
VALUES($my_val,$my_val,'$my_val','$my_val','#$my_val#');
eval SELECT COUNT(*) = 1 FROM t1 WHERE f_char1 = '$my_val';
# let $my_val= NULL;
eval INSERT INTO t1(f_int1,f_int2,f_char1,f_char2,f_charbig)
VALUES(NULL,NULL,NULL,NULL,NULL);
eval SELECT COUNT(*) = 1 FROM t1 WHERE f_char1 IS NULL;
DROP TABLE t1;
}
#
--echo #  4.   Partition by RANGE(...) subpartition by HASH(<check value>)
eval CREATE TABLE t1 (
$column_list
)
PARTITION BY RANGE(CAST(f_char1 AS SIGNED INTEGER))
SUBPARTITION BY HASH(CAST(f_char2 AS SIGNED INTEGER) * CAST(5.0E+18 AS SIGNED INTEGER)) SUBPARTITIONS 4
(PARTITION p0 VALUES LESS THAN (0),
 PARTITION p1 VALUES LESS THAN MAXVALUE);
let $my_val= 2147483646;
eval INSERT INTO t1(f_int1,f_int2,f_char1,f_char2,f_charbig)
VALUES($my_val,$my_val,'1','$my_val','#$my_val#');
eval SELECT COUNT(*) = 1 FROM t1 WHERE f_char2 = '$my_val';
let $my_val= -2147483646;
eval INSERT INTO t1(f_int1,f_int2,f_char1,f_char2,f_charbig)
VALUES($my_val,$my_val,'-1','$my_val','#$my_val#');
eval SELECT COUNT(*) = 1 FROM t1 WHERE f_char2 = '$my_val';
let $my_val= 0;
eval INSERT INTO t1(f_int1,f_int2,f_char1,f_char2,f_charbig)
VALUES($my_val,$my_val,'$my_val','$my_val','#$my_val#');
eval SELECT COUNT(*) = 1 FROM t1 WHERE f_char2 = '$my_val';
# let $my_val= NULL;
eval INSERT INTO t1(f_int1,f_int2,f_char1,f_char2,f_charbig)
VALUES(NULL,NULL,NULL,NULL,NULL);
eval SELECT COUNT(*) = 1 FROM t1 WHERE f_char2 IS NULL;
DROP TABLE t1;
#
--echo #  5.   Partition by LIST(...) subpartition by HASH(<check value>)
eval CREATE TABLE t1 (
$column_list
)
PARTITION BY LIST(CAST(f_char1 AS SIGNED INTEGER))
SUBPARTITION BY HASH(CAST(f_char2 AS SIGNED INTEGER) * CAST(5.0E+18 AS SIGNED INTEGER)) SUBPARTITIONS 4
(PARTITION p0 VALUES IN (NULL),
 PARTITION p1 VALUES IN (1));
let $my_val= 2147483646;
eval INSERT INTO t1(f_int1,f_int2,f_char1,f_char2,f_charbig)
VALUES($my_val,$my_val,'1','$my_val','#$my_val#');
eval SELECT COUNT(*) = 1 FROM t1 WHERE f_char2 = '$my_val';
let $my_val= -2147483646;
eval INSERT INTO t1(f_int1,f_int2,f_char1,f_char2,f_charbig)
VALUES($my_val,$my_val,'1','$my_val','#$my_val#');
eval SELECT COUNT(*) = 1 FROM t1 WHERE f_char2 = '$my_val';
let $my_val= 0;
eval INSERT INTO t1(f_int1,f_int2,f_char1,f_char2,f_charbig)
VALUES($my_val,$my_val,'1','$my_val','#$my_val#');
eval SELECT COUNT(*) = 1 FROM t1 WHERE f_char2 = '$my_val';
# let $my_val= NULL;
eval INSERT INTO t1(f_int1,f_int2,f_char1,f_char2,f_charbig)
VALUES(NULL,NULL,NULL,NULL,NULL);
eval SELECT COUNT(*) = 1 FROM t1 WHERE f_char2 IS NULL;
DROP TABLE t1;
#

--enable_testcase
