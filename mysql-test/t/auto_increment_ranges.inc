#
# Test of auto_increment at end of range
#
--disable_warnings
drop table if exists t1;
--enable_warnings

--echo #
--echo # Testing ranges with smallint
--echo #
let $type=smallint;
let $range_max=32767;

eval create table t1 (a $type primary key auto_increment);
eval insert into t1 values($range_max);
--error HA_ERR_AUTOINC_ERANGE
insert into t1 values(NULL);
truncate table t1;
eval insert into t1 values($range_max-1);
insert into t1 values(NULL);
--error HA_ERR_AUTOINC_ERANGE
insert into t1 values(NULL);
select * from t1;
truncate table t1;
--error HA_ERR_AUTOINC_ERANGE
eval insert into t1 values($range_max),(NULL);
select * from t1;
truncate table t1;
--error HA_ERR_AUTOINC_ERANGE
eval insert into t1 values($range_max-1),(NULL),(NULL);
truncate table t1;
eval insert into t1 values($range_max+1);
select * from t1;
--error HA_ERR_AUTOINC_ERANGE
eval insert into t1 values(NULL);
drop table t1;

--echo #
--echo # Testing ranges with unsigned smallint
--echo #

let $type=smallint unsigned;
let $range_max=65535;

eval create table t1 (a $type primary key auto_increment);
eval insert into t1 values($range_max);
--error HA_ERR_AUTOINC_ERANGE
insert into t1 values(NULL);
truncate table t1;
eval insert into t1 values($range_max-1);
insert into t1 values(NULL);
--error HA_ERR_AUTOINC_ERANGE
insert into t1 values(NULL);
select * from t1;
truncate table t1;
--error HA_ERR_AUTOINC_ERANGE
eval insert into t1 values($range_max),(NULL);
select * from t1;
truncate table t1;
--error HA_ERR_AUTOINC_ERANGE
eval insert into t1 values($range_max-1),(NULL),(NULL);
truncate table t1;
eval insert into t1 values($range_max+1);
select * from t1;
--error HA_ERR_AUTOINC_ERANGE
eval insert into t1 values(NULL);
drop table t1;

--echo #
--echo # Testing ranges with integer
--echo #

let $type=int;
let $range_max=2147483647;

eval create table t1 (a $type primary key auto_increment);
eval insert into t1 values($range_max);
--error HA_ERR_AUTOINC_ERANGE
insert into t1 values(NULL);
truncate table t1;
eval insert into t1 values($range_max-1);
insert into t1 values(NULL);
--error HA_ERR_AUTOINC_ERANGE
insert into t1 values(NULL);
select * from t1;
truncate table t1;
--error HA_ERR_AUTOINC_ERANGE
eval insert into t1 values($range_max),(NULL);
select * from t1;
truncate table t1;
--error HA_ERR_AUTOINC_ERANGE
eval insert into t1 values($range_max-1),(NULL),(NULL);
truncate table t1;
eval insert into t1 values($range_max+1);
select * from t1;
--error HA_ERR_AUTOINC_ERANGE
eval insert into t1 values(NULL);
drop table t1;

--echo #
--echo # Testing ranges with unsigned integer
--echo #

let $type=int unsigned;
let $range_max=4294967295;

eval create table t1 (a $type primary key auto_increment);
eval insert into t1 values($range_max);
--error HA_ERR_AUTOINC_ERANGE
insert into t1 values(NULL);
truncate table t1;
eval insert into t1 values($range_max-1);
insert into t1 values(NULL);
--error HA_ERR_AUTOINC_ERANGE
insert into t1 values(NULL);
select * from t1;
truncate table t1;
--error HA_ERR_AUTOINC_ERANGE
eval insert into t1 values($range_max),(NULL);
select * from t1;
truncate table t1;
--error HA_ERR_AUTOINC_ERANGE
eval insert into t1 values($range_max-1),(NULL),(NULL);
truncate table t1;
eval insert into t1 values($range_max+1);
select * from t1;
--error HA_ERR_AUTOINC_ERANGE
eval insert into t1 values(NULL);
drop table t1;

--echo #
--echo # Testing ranges with bigint
--echo #

let $type=bigint;
let $range_max=cast(9223372036854775807 as unsigned);

eval create table t1 (a $type primary key auto_increment);
eval insert into t1 values($range_max);
--error HA_ERR_AUTOINC_ERANGE
insert into t1 values(NULL);
truncate table t1;
eval insert into t1 values($range_max-1);
insert into t1 values(NULL);
--error HA_ERR_AUTOINC_ERANGE
insert into t1 values(NULL);
select * from t1;
truncate table t1;
--error HA_ERR_AUTOINC_ERANGE
eval insert into t1 values($range_max),(NULL);
select * from t1;
truncate table t1;
--error HA_ERR_AUTOINC_ERANGE
eval insert into t1 values($range_max-1),(NULL),(NULL);
truncate table t1;
eval insert into t1 values($range_max+1);
select * from t1;
--error HA_ERR_AUTOINC_ERANGE
eval insert into t1 values(NULL);
drop table t1;

--echo #
--echo # Testing ranges with unsigned bigint
--echo #

let $type=bigint unsigned;
let $range_max=18446744073709551615;

eval create table t1 (a $type primary key auto_increment);
eval insert into t1 values($range_max-1);
--error ER_AUTOINC_READ_FAILED
insert into t1 values(NULL);
--error ER_AUTOINC_READ_FAILED
insert into t1 values(NULL);
truncate table t1;
eval insert into t1 values($range_max-1);
--error ER_AUTOINC_READ_FAILED
insert into t1 values(NULL);
--error ER_AUTOINC_READ_FAILED
insert into t1 values(NULL);
select * from t1;
truncate table t1;
--error ER_AUTOINC_READ_FAILED
eval insert into t1 values($range_max),(NULL);
select * from t1;
truncate table t1;
--error ER_AUTOINC_READ_FAILED
eval insert into t1 values($range_max-1),(NULL),(NULL);
drop table t1;

--echo #
--echo # Test IGNORE and strict mode
--echo #
create table t1 (a smallint primary key auto_increment);
insert ignore into t1 values(32766),(NULL),(NULL),(1);
select * from t1;
truncate table t1;

set @org_mode=@@sql_mode;
set @@sql_mode='ansi,traditional';
insert ignore into t1 values(32766),(NULL),(NULL);
truncate table t1;
--error HA_ERR_AUTOINC_ERANGE
insert into t1 values(32766),(NULL),(NULL);
set @@sql_mode=@org_mode;
drop table t1;

--echo #
--echo # Test auto increment with negative numbers
--echo #
CREATE TABLE t1 (a INTEGER AUTO_INCREMENT, PRIMARY KEY (a));
INSERT INTO t1 VALUES (NULL), (2), (-5), (NULL);
INSERT INTO t1 VALUES (NULL);
SELECT * FROM t1;
TRUNCATE TABLE t1;
INSERT INTO t1 VALUES (-5), (NULL);
SELECT * FROM t1;
DROP TABLE t1;

--echo #
--echo # Test inserting a value out-of-range into an auto increment column
--echo #
CREATE TABLE t1 (a smallint AUTO_INCREMENT, PRIMARY KEY (a));
INSERT INTO t1 VALUES (2);
INSERT INTO t1 VALUES (32768);
--error HA_ERR_AUTOINC_ERANGE
INSERT INTO t1 VALUES (NULL);
SELECT * FROM t1;
DROP TABLE t1;


--echo #
--echo # Test old behaviour
--echo #
create table t1 (a smallint primary key auto_increment);
insert into t1 values(32766),(NULL);
delete from t1 where a=32767;
--error HA_ERR_AUTOINC_ERANGE
insert into t1 values(NULL);
drop table t1;
