#
# Bug when using comparions of strings and integers.
#

--disable_warnings
drop table if exists t1;
--enable_warnings

CREATE TABLE t1 (id CHAR(12) not null, PRIMARY KEY (id));
insert into t1 values ('000000000001'),('000000000002');
analyze table t1;
explain select * from t1 where id=000000000001;
select * from t1 where id=000000000001;
delete from t1 where id=000000000002;
select * from t1;
drop table t1;

#
# Check the following:
# "a"  == "a "
# "a\0" < "a"
# "a\0" < "a "

set names latin1;
SELECT 'a' = 'a ';
SELECT 'a\0' < 'a';
SELECT 'a\0' < 'a ';
SELECT 'a\t' < 'a';
SELECT 'a\t' < 'a ';

CREATE TABLE t1 (a char(10) not null) charset latin1;
INSERT INTO t1 VALUES ('a'),('a\0'),('a\t'),('a ');
SELECT hex(a),STRCMP(a,'a'), STRCMP(a,'a ') FROM t1;
DROP TABLE t1;
set names default;

# Bug #8134: Comparison against CHAR(31) at end of string
SELECT CHAR(31) = '', '' = CHAR(31);
# Extra test
SELECT CHAR(30) = '', '' = CHAR(30);

# End of 4.1 tests

#
#Bug #21159: Optimizer: wrong result after AND with different data types
#
create table t1 (a tinyint(1),b binary(1));
insert into t1 values (0x01,0x01);
select * from t1 where a=b;
select * from t1 where a=b and b=0x01;
drop table if exists t1;

#
# Bug #31887: DML Select statement not returning same results when executed
# in version 5
#

CREATE TABLE  t1 (b int(2) zerofill, c int(2) zerofill);
INSERT INTO t1 (b,c) VALUES (1,2), (1,1), (2,2);

SELECT CONCAT(b,c), CONCAT(b,c) = '0101' FROM t1;

analyze table t1;
--replace_column 10 x
EXPLAIN SELECT b,c FROM t1 WHERE b = 1 AND CONCAT(b,c) = '0101';
SELECT b,c FROM t1 WHERE b = 1 AND CONCAT(b,c) = '0101';

CREATE TABLE t2 (a int);
INSERT INTO t2 VALUES (1),(2);

SELECT a, 
  (SELECT COUNT(*) FROM t1 
   WHERE b = t2.a AND CONCAT(b,c) = CONCAT('0',t2.a,'01')) x 
FROM t2 ORDER BY a;

analyze table t1;
analyze table t2;
--replace_column 10 x
EXPLAIN 
SELECT a, 
  (SELECT COUNT(*) FROM t1 
   WHERE b = t2.a AND CONCAT(b,c) = CONCAT('0',t2.a,'01')) x 
FROM t2 ORDER BY a;

DROP TABLE t1,t2;

#
# Bug #39353: Multiple conditions on timestamp column crashes server
#
SET sql_mode = 'NO_ENGINE_SUBSTITUTION';
CREATE TABLE t1 (a TIMESTAMP); 
INSERT INTO t1 VALUES (NOW()),(NOW()),(NOW());
SELECT * FROM t1 WHERE a > '2008-01-01' AND a = '0000-00-00';
DROP TABLE t1;
SET sql_mode = default;
--echo End of 5.0 tests

#
# Bug #11764818 57692: Crash in item_func_in::val_int() with ZEROFILL
#

CREATE TABLE t1(a INT ZEROFILL);
SELECT 1 FROM t1 WHERE t1.a IN (1, t1.a) AND t1.a=2;
DROP TABLE t1;

--echo
--echo Bug #27452148: ASSERTION FAILED: (SLEN % 4) == 0 IN
--echo                MY_STRNNCOLLSP_UTF32_BIN
--echo
SET NAMES latin1;
CREATE TABLE t1(a CHAR(10) CHARACTER SET utf32 COLLATE utf32_bin);
INSERT INTO t1 VALUES('a'),('b'),('c');
SELECT ROW('1', '1') > ROW(a, '1') FROM t1;
SELECT ROW(a, '1') > ROW('1', '1') FROM t1;
DROP TABLE t1;
SET NAMES default;

--echo #
--echo # Bug#30961924 AND OF TWO PREDICATES WITH DIFFERENT COLLATION ON SAME COLUMN GIVES WRONG RESULT
--echo #

CREATE TABLE t1(
  firstname CHAR(20),
  lastname CHAR(20)
) CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci;

INSERT INTO t1 VALUES("john","doe"), ("John","Doe");

# Case-insensitive comparison, john is John
SELECT * FROM t1 WHERE firstname = 'john';
SELECT * FROM t1 WHERE firstname = 'john' COLLATE utf8mb4_0900_ai_ci;
# Case-insensitive comparison, john is not John
SELECT * FROM t1 WHERE firstname = 'john' COLLATE utf8mb4_0900_as_cs;
SELECT * FROM t1
WHERE firstname = 'john' COLLATE utf8mb4_0900_ai_ci AND
      firstname = 'john' COLLATE utf8mb4_0900_as_cs;
SELECT * FROM t1
WHERE firstname = 'john' COLLATE utf8mb4_0900_as_cs AND
      firstname = 'john' COLLATE utf8mb4_0900_ai_ci;
DROP TABLE t1;

--echo Comparisons with parameters and extreme values

CREATE TABLE t(a DECIMAL(30,0));
INSERT INTO t VALUES(0);
SELECT * FROM t;
set @maxint=18446744073709551615;
SELECT @maxint;

# was wrong empty
SELECT * FROM t WHERE a < @maxint;
SELECT * FROM t WHERE a < 18446744073709551615;
SELECT * FROM t WHERE 0 < 18446744073709551615;
SELECT * FROM t WHERE 0 < @maxint;
DROP TABLE t;

--echo #
--echo # Bug#35105475: Result diff seen with hypergraph off and on
--echo # (different number of rows)
--echo #

CREATE TABLE t(i INT, x VARCHAR(1));
INSERT INTO t VALUES (0, '4'), (0, _utf16 X'1BB4');
--disable_warnings
--sorted_result
SELECT t1.i, HEX(t1.x), t2.i, HEX(t2.x) FROM t AS t1, t AS t2
WHERE t1.x = t2.x and t1.i < t2.x;
--enable_warnings
DROP TABLE t;
