SET sql_mode = 'NO_ENGINE_SUBSTITUTION';

#
# The last select returned wrong results in 3.23.52
#

create table t1 (id int);
insert into t1 values (null), (0);
create table t2 (id int);
insert into t2 values (null);
select * from t1, t2 where t1.id = t2.id;
alter table t1 add key id (id);
select * from t1, t2 where t1.id = t2.id;
drop table t1,t2;

#
# Check bug when doing <=> NULL on an indexed null field
#

create table t1 (
  id  integer,
  id2 integer not null,
  index (id),
  index (id2)
);
insert into t1 values(null,null),(1,1);
select * from t1; 
select * from t1 where id <=> null;
select * from t1 where id <=> null or id > 0;
select * from t1 where id is null or id > 0;
select * from t1 where id2 <=> null or id2 > 0;
select * from t1 where id2 is null or id2 > 0;
delete from t1 where id <=> NULL;
select * from t1; 
drop table t1; 

#
# Test for bug #12144: optimizations for key access with null keys
#                      used for outer joins
#

CREATE TABLE t1 (a int);
CREATE TABLE t2 (a int, b int, INDEX idx(a));
CREATE TABLE t3 (b int, INDEX idx(b));
CREATE TABLE t4 (b int, INDEX idx(b));
INSERT INTO t1 VALUES (1), (2), (3), (4);
INSERT INTO t2 VALUES (1, 1), (3, 1);
INSERT INTO t3 VALUES
  (NULL), (NULL), (NULL), (NULL), (NULL),
  (NULL), (NULL), (NULL), (NULL), (NULL);
INSERT INTO t4 SELECT * FROM t3;
INSERT INTO t3 SELECT * FROM t4;
INSERT INTO t4 SELECT * FROM t3;
INSERT INTO t3 SELECT * FROM t4;
INSERT INTO t4 SELECT * FROM t3;
INSERT INTO t3 SELECT * FROM t4;
INSERT INTO t4 SELECT * FROM t3;
INSERT INTO t3 SELECT * FROM t4;
INSERT INTO t4 SELECT * FROM t3;
INSERT INTO t3 SELECT * FROM t4;
INSERT INTO t4 SELECT * FROM t3;
INSERT INTO t3 SELECT * FROM t4;
INSERT INTO t4 SELECT * FROM t3;
INSERT INTO t3 SELECT * FROM t4;
INSERT INTO t4 SELECT * FROM t3;
INSERT INTO t3 SELECT * FROM t4;
INSERT INTO t3 VALUES (2), (3);

ANALYZE table t1, t2, t3;

SELECT COUNT(*) FROM t3;

--replace_column 10 #
EXPLAIN SELECT SQL_CALC_FOUND_ROWS * FROM t1 LEFT JOIN t2 ON t1.a=t2.a
                                             LEFT JOIN t3 ON t2.b=t3.b;
FLUSH STATUS ;
--sorted_result
--replace_column 10 #
SELECT SQL_CALC_FOUND_ROWS * FROM t1 LEFT JOIN t2 ON t1.a=t2.a
                                     LEFT JOIN t3 ON t2.b=t3.b;
SELECT FOUND_ROWS();
--skip_if_hypergraph  # Depends on the query plan.
SHOW STATUS LIKE "handler_read%";

DROP TABLE t1,t2,t3,t4;
# End of 4.1 tests

#
# BUG#34945 "ref_or_null queries that are null_rejecting and have a null value crash mysql"
#
CREATE TABLE t1 (
  a int(11) default NULL,
  b int(11) default NULL,
  KEY a (a,b)
);
INSERT INTO t1 VALUES (0,10),(0,11),(0,12);

CREATE TABLE t2 (
  a int(11) default NULL,
  b int(11) default NULL,
  KEY a (a)
);
INSERT INTO t2 VALUES (3,NULL),(3,11),(3,12);

SELECT * FROM t2 inner join t1 WHERE ( t1.a = 0 OR t1.a IS NULL) AND t2.a = 3 AND t2.b = t1.b;

drop table t1, t2;
-- echo End of 5.0 tests

--echo #
--echo # Bug#54608 Query using IN + OR + IS TRUE and IS NULL returns
--echo # NULL when should be empty
--echo #

CREATE TABLE t1 (a INT, KEY (a));
INSERT INTO t1 VALUES (1), (2), (NULL);
explain SELECT a FROM t1 WHERE a IN (42) OR (a IS TRUE AND a IS NULL);
SELECT a FROM t1 WHERE a IN (42) OR (a IS TRUE AND a IS NULL);
--replace_column 10 #
explain SELECT a FROM t1 WHERE a IN (42) OR (a=NULL);
SELECT a FROM t1 WHERE a IN (42) OR (a=NULL);
drop table t1;
SET sql_mode = default;

