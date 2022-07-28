show variables like 'collation_server';
Variable_name	Value
collation_server	ucs2_unicode_ci
show variables like "%character_set_ser%";
Variable_name	Value
character_set_server	ucs2
DROP TABLE IF EXISTS t1;
create table t1 (a int);
drop table t1;
End of 4.1 tests
create table t1 (a char(1) character set latin1);
insert into t1 values ('a'),('b'),('c');
select hex(group_concat(a)) from t1;
hex(group_concat(a))
612C622C63
drop table t1;
CREATE TABLE t1(col1 VARCHAR(32) CHARACTER SET ucs2 COLLATE ucs2_bin NOT NULL, 
col2 VARCHAR(32) CHARACTER SET ucs2 COLLATE ucs2_bin NOT NULL, 
UNIQUE KEY key1 USING HASH (col1, col2)) ENGINE=MEMORY;
Warnings:
Warning	1287	'ucs2' is deprecated and will be removed in a future release. Please use utf8mb4 instead
Warning	4079	'ucs2_bin' is a collation of the deprecated character set ucs2. Please consider using utf8mb4 with an appropriate collation instead.
Warning	1287	'ucs2' is deprecated and will be removed in a future release. Please use utf8mb4 instead
Warning	4079	'ucs2_bin' is a collation of the deprecated character set ucs2. Please consider using utf8mb4 with an appropriate collation instead.
INSERT INTO t1 VALUES('A', 'A'), ('B', 'B'), ('C', 'C');
INSERT INTO t1 VALUES('A ', 'A ');
ERROR 23000: Duplicate entry 'A -A ' for key 't1.key1'
DROP TABLE t1;
End of 5.0 tests
