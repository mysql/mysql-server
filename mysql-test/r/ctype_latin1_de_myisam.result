set names latin1;
ALTER SCHEMA test COLLATE latin1_german2_ci;
set @@collation_connection=latin1_german2_ci;
select @@collation_connection;
@@collation_connection
latin1_german2_ci
drop table if exists t1;
create table t1 (a varchar(10), key(a), fulltext (a)) engine=myisam;
insert into t1 values ("a"),("abc"),("abcd"),("hello"),("test");
select * from t1 where a like "abc%";
a
abc
abcd
select * from t1 where a like "test%";
a
test
select * from t1 where a like "te_t";
a
test
select * from t1 where match a against ("te*" in boolean mode)+0;
a
test
drop table t1;
SET NAMES latin1;
CREATE TABLE t1 (
col1 varchar(255) NOT NULL default ''
) ENGINE=MyISAM DEFAULT CHARSET=latin1 collate latin1_german2_ci;
INSERT INTO t1 VALUES ('ß'),('ss'),('ss');
ALTER TABLE t1 ADD KEY ifword(col1);
SELECT * FROM t1 WHERE col1='ß' ORDER BY col1, BINARY col1;
col1
ß
Warnings:
Warning	1287	'BINARY expr' is deprecated and will be removed in a future release. Please use CAST instead
DROP TABLE t1;
ALTER SCHEMA test COLLATE utf8mb4_0900_ai_ci;
