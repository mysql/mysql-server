################################################################################
# inc/gcol_partition.inc                                                       #
#                                                                              #
# Purpose:                                                                     #
#  Testing partitioning tables with generated columns.                           #
#                                                                              #
#                                                                              #
#                                                                              #
#------------------------------------------------------------------------------#
# Original Author: Andrey Zhakov                                               #
# Original Date: 2008-09-04                                                    #
# Change Author:                                                               #
# Change Date:                                                                 #
# Change:                                                                      #
################################################################################


--disable_warnings
drop table if exists t1;
--enable_warnings

--echo # Case 1. Partitioning by RANGE based on a non-stored generated column.

CREATE TABLE t1 (
    a DATE NOT NULL,
    b int generated always as (year(a)) virtual
)
PARTITION BY RANGE( b ) (
    PARTITION p0 VALUES LESS THAN (2006),
    PARTITION p2 VALUES LESS THAN (2008)
);

insert into t1 values ('2006-01-01',default);
insert into t1 values ('2007-01-01',default);
insert into t1 values ('2005-01-01',default);
select * from t1;

# Specifically for MyISAM, check that data is written into correct 
# $MYSQLTEST_VARDIR/master-data/test/t1*p?.MYD files

--echo # Modify the expression of generated column b
ALTER TABLE t1 modify b int generated always as (year(a)-1) virtual;

select * from t1;

drop table t1;

--echo # Case 2. Partitioning by LIST based on a stored generated column.

CREATE TABLE t1 (a int, b int generated always as (a % 3 ) stored)
PARTITION BY LIST (a+1)
(PARTITION p1 VALUES IN (1), PARTITION p2 VALUES IN (2));

insert into t1 values (1,default);
select * from t1;

#
# NOTE: The following tests are currently failing due to a
#       [suspected] bug in the existing partition functionality.
#       Here is what was observed when using mysqld compiled prior 
#       to adding the generated column functionality.
#         mysql> create table t1 (a int) partition by list (a) 
#                (partition p1 values in (1), partition p2 values in (2));
#         Query OK, 0 rows affected (0.00 sec)
#
#         mysql> insert into t1 values (1), (1), (2);                                                                   
#         Query OK, 3 rows affected (0.00 sec)
#         Records: 3  Duplicates: 0  Warnings: 0
#
#         mysql> select * from t1;                                                                                      
#         +------+
#         | a    |
#         +------+
#         |    1 | 
#         |    1 | 
#         |    2 | 
#         +------+
#         3 rows in set (0.00 sec)
#       
#         mysql> alter table t1 reorganize partition p1 into 
#                (partition p1 values in (3));                              
#         Query OK, 2 rows affected (3.90 sec)
#         Records: 2  Duplicates: 2  Warnings: 0
#         
#         mysql> select * from t1;                                                        
#         +------+
#         | a    |
#         +------+
#         |    2 |             <- Two row have been lost!!! 
#         +------+
#         1 row in set (0.00 sec)

#
#alter table t1 change b b virtual int as ((a % 3)+1) stored;
#--error ER_NO_PARTITION_FOR_GIVEN_VALUE
#alter table t1 change b b virtual int as (a % 2) stored;
#if ($myisam_engine)
#{
# --echo # Check how data is physically partitioned.
# --replace_result $MYSQLTEST_VARDIR MYSQLTEST_VARDIR
# --exec du -b $MYSQLTEST_VARDIR/master-data/test/t1*p?.MYD
#}

select * from t1;

drop table t1;

--echo # Case 3. Partitioning by HASH based on a non-stored generated column.

CREATE TABLE t1 (
    a DATE NOT NULL,
    b int generated always as (year(a)) virtual
)
PARTITION BY HASH( b % 3 ) PARTITIONS 3;

insert into t1 values ('2005-01-01',default);
insert into t1 values ('2006-01-01',default);
select * from t1;

--echo # Modify the expression of generated column b
ALTER TABLE t1 modify b int generated always as (year(a)-1) virtual;

select * from t1;

drop table t1;

--echo #
--echo # Bug#21779011 INVALID READS AND SENDING RANDOM SERVER MEMORY BACK
--echo #              TO CLIENT
--echo #

CREATE TABLE t (
 c INTEGER GENERATED ALWAYS AS (2) VIRTUAL,
 d INTEGER,
 KEY (d)
) PARTITION BY KEY (d) PARTITIONS 2;

INSERT INTO t (d) VALUES (1),(1),(2),(2);

# Need to disable ONLY_FULL_GROUP_BY mode
--source include/turn_off_only_full_group_by.inc

SELECT c FROM t WHERE d >= 1 GROUP BY d LIMIT 2;

--source include/restore_sql_mode_after_turn_off_only_full_group_by.inc

DROP TABLE t;

--echo #
--echo # Bug#21779554: CHECK_MISPLACED_ROWS BOGUS "FOUND A MISPLACED ROW"
--echo #               AND CRASHES
--echo #
CREATE TABLE t(a INT,b INT GENERATED ALWAYS AS (1) VIRTUAL,c INT)
PARTITION BY KEY (b)PARTITIONS 6;
INSERT INTO t VALUES();
CHECK TABLE t EXTENDED;
FLUSH TABLES;
CHECK TABLE t EXTENDED;
DROP TABLE t;

--echo #
--echo # Bug#22574695 ASSERTION `!TABLE || (!TABLE->READ_SET || BITMAP_IS_SET(TABLE->READ_SET, FIELD_I
--echo #

CREATE TABLE t1(
   col1 VARCHAR(500),
   col2 INT,
   col5 VARCHAR(500) GENERATED ALWAYS AS
        (SUBSTR(col1,1,500)) VIRTUAL
) ENGINE = InnoDB PARTITION BY KEY(col5) PARTITIONS 2;

INSERT INTO test . t1 SET col1 = REPEAT('_', 500);
COMMIT;
UPDATE t1 SET col2 = 1;
SELECT 'Obviously no crash';

DROP TABLE t1;
