################################################################################
# inc/vcol_partition.inc                                                       #
#                                                                              #
# Purpose:                                                                     #
#  Testing partitioning tables with virtual columns.                           #
#                                                                              #
#                                                                              #
#                                                                              #
#------------------------------------------------------------------------------#
# Original Author: Andrey Zhakov                                               #
# Original Date: 2008-09-04                                                    #
# Change Author: Oleksandr Byelkin (Monty program Ab)
# Date: 2009-03-24 
# Change: Syntax changed
################################################################################

--source include/have_partition.inc

--disable_warnings
drop table if exists t1;
--enable_warnings

--echo # Case 1. Partitioning by RANGE based on a non-stored virtual column.

CREATE TABLE t1 (
    a DATE NOT NULL,
    b int as (year(a))
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
# $MYSQLTEST_VARDIR/mysqld.1/data/test/t1*p?.MYD files
let $myisam_engine = `select @@session.storage_engine='myisam'`;
if ($myisam_engine)
{
 --echo # Check how data is physically partitioned.
 --replace_result $MYSQLTEST_VARDIR MYSQLTEST_VARDIR
 --exec du -b $MYSQLTEST_VARDIR/mysqld.1/data/test/t1*p?.MYD
}

--echo # Modify the expression of virtual column b
ALTER TABLE t1 modify b int as (year(a)-1);

select * from t1;

if ($myisam_engine)
{
 --echo # Check how data is physically partitioned.
 --replace_result $MYSQLTEST_VARDIR MYSQLTEST_VARDIR
 --exec du -b $MYSQLTEST_VARDIR/mysqld.1/data/test/t1*p?.MYD
}

drop table t1;

--echo # Case 2. Partitioning by LIST based on a stored virtual column.

CREATE TABLE t1 (a int, b int as (a % 3 ) persistent)
PARTITION BY LIST (a+1)
(PARTITION p1 VALUES IN (1), PARTITION p2 VALUES IN (2));

insert into t1 values (1,default);
if ($myisam_engine)
{
 --echo # Check how data is physically partitioned.
 --replace_result $MYSQLTEST_VARDIR MYSQLTEST_VARDIR
 --exec du -b $MYSQLTEST_VARDIR/mysqld.1/data/test/t1*p?.MYD
}
select * from t1;

#
# NOTE: The following tests are currently failing due to a
#       [suspected] bug in the existing partition functionality.
#       Here is what was observed when using mysqld compiled prior 
#       to adding the virtual column functionality.
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
#alter table t1 change b b int as ((a % 3)+1) persistent;
#--error ER_NO_PARTITION_FOR_GIVEN_VALUE
#alter table t1 change b b int as (a % 2) persistent;
#if ($myisam_engine)
#{
# --echo # Check how data is physically partitioned.
# --replace_result $MYSQLTEST_VARDIR MYSQLTEST_VARDIR
# --exec du -b $MYSQLTEST_VARDIR/mysqld.1/data/test/t1*p?.MYD
#}

select * from t1;

drop table t1;

--echo # Case 3. Partitioning by HASH based on a non-stored virtual column.

CREATE TABLE t1 (
    a DATE NOT NULL,
    b int as (year(a))
)
PARTITION BY HASH( b % 3 ) PARTITIONS 3;

insert into t1 values ('2005-01-01',default);
insert into t1 values ('2006-01-01',default);
select * from t1;

if ($myisam_engine)
{
 --echo # Check how data is physically partitioned.
 --replace_result $MYSQLTEST_VARDIR MYSQLTEST_VARDIR
 --exec du -b $MYSQLTEST_VARDIR/mysqld.1/data/test/t1*p?.MYD
}

--echo # Modify the expression of virtual column b
ALTER TABLE t1 modify b int as (year(a)-1);

select * from t1;

if ($myisam_engine)
{
 --echo # Check how data is physically partitioned.
 --replace_result $MYSQLTEST_VARDIR MYSQLTEST_VARDIR
 --exec du -b $MYSQLTEST_VARDIR/mysqld.1/data/test/t1*p?.MYD
}

drop table t1;
