** Setup **

SET @session_sql_big_selects = @@SESSION.sql_big_selects;
SET @session_max_join_size = @@SESSION.max_join_size;
SET @global_max_join_size = @@GLOBAL.max_join_size;
SET MAX_JOIN_SIZE=4;
CREATE TEMPORARY TABLE t1(a varchar(20) not null, b varchar(20));
CREATE TEMPORARY TABLE t2(a varchar(20) null, b varchar(20));
INSERT INTO t1 VALUES('aa','bb');
INSERT INTO t1 VALUES('aa1','bb');
INSERT INTO t1 VALUES('aa2','bb');
INSERT INTO t1 VALUES('aa3','bb');
INSERT INTO t1 VALUES('aa4','bb');
INSERT INTO t2 VALUES('aa','bb');
INSERT INTO t2 VALUES('aa1','bb');
INSERT INTO t2 VALUES('aa2','bb');
INSERT INTO t2 VALUES('aa3','bb');
INSERT INTO t2 VALUES('aa4','bb');
'#--------------------FN_DYNVARS_154_01-------------------------#'
Expected error "Too big select"
SELECT * FROM t1 INNER JOIN t2 ON t1.a = t2.a;
ERROR 42000: The SELECT would examine more than MAX_JOIN_SIZE rows; check your WHERE and use SET SQL_BIG_SELECTS=1 or SET MAX_JOIN_SIZE=# if the SELECT is okay
Expected error The SELECT would examine more than MAX_JOIN_SIZE rows.
'#--------------------FN_DYNVARS_154_02-------------------------#'
SET SESSION SQL_BIG_SELECTS = 1;
SELECT * FROM t1 INNER JOIN t2 ON t1.a = t2.a;
a	b	a	b
aa	bb	aa	bb
aa1	bb	aa1	bb
aa2	bb	aa2	bb
aa3	bb	aa3	bb
aa4	bb	aa4	bb
This should work
SET SESSION SQL_BIG_SELECTS = 0;
DELETE FROM t2 WHERE a = 'aa4';
SELECT * FROM t1 INNER JOIN t2 ON t1.a = t2.a;
a	b	a	b
aa	bb	aa	bb
aa1	bb	aa1	bb
aa2	bb	aa2	bb
aa3	bb	aa3	bb
This should work
'#--------------------FN_DYNVARS_154_03-------------------------#'
** Connecting con_int1 using root **
** Connection con_int1 **
SELECT @@SESSION.sql_big_selects;
@@SESSION.sql_big_selects
1
1 Expected
SET SESSION sql_big_selects = 0;
** Connecting con_int2 using root **
** Connection con_int2 **
SELECT @@SESSION.sql_big_selects;
@@SESSION.sql_big_selects
1
1 Expected
SET SESSION sql_big_selects = 1;
** Connection con_int1 **
SELECT @@SESSION.sql_big_selects;
@@SESSION.sql_big_selects
0
0 Expected
** Connection con_int2 **
SELECT @@SESSION.sql_big_selects;
@@SESSION.sql_big_selects
1
1 Expected
** Connection default **
Disconnecting Connections con_int1, con_int2
SET @@SESSION.sql_big_selects = @session_sql_big_selects;
SET @@SESSION.max_join_size = @session_max_join_size;
SET @@GLOBAL.max_join_size = @global_max_join_size;
DROP TABLE t1;
DROP TABLE t2;
