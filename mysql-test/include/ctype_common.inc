#
# Common tests for all character sets and collations.
# Include this file from a test with @test_characrer_set
# and @test_collation set to desired values.
#
# Please don't use SHOW CREATE TABLE in this file,
# we want it to be HANDLER independent. You can
# use SHOW FULL COLUMNS instead.
#
# Please surround all CREATE TABLE with --disable_warnings
# and --enable_warnings to be able to set storage_engine
# without having to check if the hanlder exists.

SET @safe_character_set_server= @@character_set_server;
SET @safe_collation_server= @@collation_server;
SET character_set_server= @test_character_set;
SET collation_server= @test_collation;
CREATE DATABASE d1;
USE d1;

#
# Bug 1883: LIKE did not work in some cases with a key.
#
--disable_warnings
CREATE TABLE t1 (c CHAR(10), KEY(c));
--enable_warnings
# check the column was created with the expected charset/collation
SHOW FULL COLUMNS FROM t1;
INSERT INTO t1 VALUES ('aaa'),('aaaa'),('aaaaa');
SELECT c as want3results FROM t1 WHERE c LIKE 'aaa%';
DROP TABLE t1;

#
# Bug 6643 incorrect response with partial utf8 index
#
--disable_warnings
CREATE TABLE t1 (c1 varchar(15), KEY c1 (c1(2)));
--enable_warnings
# check the column was created with the expected charset/collation
SHOW FULL COLUMNS FROM t1;
INSERT INTO t1 VALUES ('location'),('loberge'),('lotre'),('boabab');
SELECT c1 as want3results from t1 where c1 like 'l%';
SELECT c1 as want3results from t1 where c1 like 'lo%';
SELECT c1 as want1result  from t1 where c1 like 'loc%';
SELECT c1 as want1result  from t1 where c1 like 'loca%';
SELECT c1 as want1result  from t1 where c1 like 'locat%';
SELECT c1 as want1result  from t1 where c1 like 'locati%';
SELECT c1 as want1result  from t1 where c1 like 'locatio%';
SELECT c1 as want1result  from t1 where c1 like 'location%';
DROP TABLE t1;

DROP DATABASE d1;
# Restore settings
USE test;
SET character_set_server= @safe_character_set_server;
SET collation_server= @safe_collation_server;
