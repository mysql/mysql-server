
# Initialise
--disable_warnings
drop table if exists t1;
--enable_warnings

set @@session.sql_auto_is_null=1;

#
# Test some ODBC compatibility
#

select {fn length("hello")}, { date "1997-10-20" };

#
# Test retreiving row with last insert_id value.
#

create table t1 (a int not null auto_increment,b int not null,primary key (a,b));
insert into t1 SET A=NULL,B=1;
insert into t1 SET a=null,b=2;
select * from t1 where a is null and b=2;
select * from t1 where a is null;
analyze table t1;
explain select * from t1 where b is null;
drop table t1;

#
# Bug #14553: NULL in WHERE resets LAST_INSERT_ID
#
CREATE TABLE t1 (a INT AUTO_INCREMENT PRIMARY KEY);
INSERT INTO t1 VALUES (NULL);
SELECT a, last_insert_id() FROM t1 WHERE a IS NULL;
SELECT a, last_insert_id() FROM t1 WHERE a IS NULL;
SELECT a, last_insert_id() FROM t1;
DROP TABLE t1;

# End of 4.1 tests

--echo #
--echo # Bug#29171668: ODBC SUBSTITUTION FOR LAST_INSERT_ID() TRIGGERS
--echo #               A NON-DETERMINISTIC TRANSFORMATION
--echo #
CREATE TABLE t1(a BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY);
CREATE TABLE t2(b INT);
INSERT INTO t2 VALUES (2),(3),(4);

# Test that the transformation doesn't happen when the auto-increment
# column is in the inner table of an outer join. (For additional test
# coverage only. Not changed by the bug fix.)
INSERT INTO t1 VALUES ();
SELECT * FROM t2 LEFT JOIN t1 ON a=b WHERE a IS NULL;
INSERT INTO t1 VALUES ();
SELECT * FROM t2 LEFT JOIN t1 ON a=b WHERE a IS NULL;

# Only rewrite WHERE, leave HAVING and the select list as they are.
# (For additional coverage. Not changed by the bug fix.)
INSERT INTO t1 VALUES ();
SELECT * FROM t1 HAVING a IS NULL;
SELECT a, a IS NULL FROM t1 WHERE a IS NULL;

# Test that the substitution is done on subsequent executions, not
# only on the first one.
SELECT * FROM t1 WHERE a IS NULL;
SELECT * FROM t1 WHERE a IS NULL;

# Test that the transformation also works for auto-increment values
# outside of the signed bigint range.
INSERT INTO t1 VALUES (9223372036854775807); # max signed bigint
INSERT INTO t1 VALUES ();
SELECT * FROM t1 WHERE a IS NULL;

DROP TABLE t1, t2;

set @@session.sql_auto_is_null=default;
