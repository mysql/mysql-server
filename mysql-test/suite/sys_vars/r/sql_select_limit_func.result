** Setup **

SET @session_sql_select_limit = @@SESSION.sql_select_limit;
SET @global_sql_select_limit = @@GLOBAL.sql_select_limit;
CREATE TEMPORARY TABLE t1(a int PRIMARY KEY, b varchar(20));
CREATE TEMPORARY TABLE t2(a int PRIMARY KEY, b varchar(20));
CREATE TEMPORARY TABLE t3(a int PRIMARY KEY, b varchar(20));
INSERT INTO t1 VALUES(1, 'val1');
INSERT INTO t1 VALUES(2, 'val2');
INSERT INTO t1 VALUES(3, 'val3');
INSERT INTO t1 VALUES(4, 'val4');
INSERT INTO t1 VALUES(5, 'val5');
INSERT INTO t1 VALUES(6, 'val6');
INSERT INTO t1 VALUES(7, 'val7');
INSERT INTO t1 VALUES(8, 'val8');
INSERT INTO t1 VALUES(9, 'val9');
INSERT INTO t2 VALUES(5, 'val5');
INSERT INTO t2 VALUES(6, 'val6');
INSERT INTO t2 VALUES(7, 'val7');
INSERT INTO t2 VALUES(8, 'val8');
INSERT INTO t2 VALUES(9, 'val9');
'#-----------------------------FN_DYNVARS_165_01-----------------#'

Value DEFAULT

SET SESSION sql_select_limit = DEFAULT;
affected rows: 0
SELECT * FROM t1;
a	b
1	val1
2	val2
3	val3
4	val4
5	val5
6	val6
7	val7
8	val8
9	val9
affected rows: 9
Expecting affected rows: 9
SELECT * FROM t1 INNER JOIN t2 ON t1.a = t2.a;
a	b	a	b
5	val5	5	val5
6	val6	6	val6
7	val7	7	val7
8	val8	8	val8
9	val9	9	val9
affected rows: 5
Expecting affected rows: 5
'#-----------------------------FN_DYNVARS_165_02---------------#'
SET SESSION sql_select_limit = 2;
affected rows: 0
SELECT * FROM t1;
a	b
1	val1
2	val2
affected rows: 2
Expecting affected rows: 2
SELECT * FROM t1 INNER JOIN t2 ON t1.a = t2.a;
a	b	a	b
5	val5	5	val5
6	val6	6	val6
affected rows: 2
Expecting affected rows: 2
SET SESSION sql_select_limit = 0;
affected rows: 0
SELECT * FROM t1;
a	b
affected rows: 0
Expecting affected rows: 0
SELECT * FROM t1 INNER JOIN t2 ON t1.a = t2.a;
a	b	a	b
affected rows: 0
Expecting affected rows: 0
'#-----------------------------FN_DYNVARS_165_03---------------#'
SET SESSION sql_select_limit = 2;
affected rows: 0
SELECT * FROM t1 LIMIT 4;
a	b
1	val1
2	val2
3	val3
4	val4
affected rows: 4
Expecting affected rows: 4
SELECT * FROM t1 INNER JOIN t2 ON t1.a = t2.a LIMIT 3;
a	b	a	b
5	val5	5	val5
6	val6	6	val6
7	val7	7	val7
affected rows: 3
Expecting affected rows: 3
'#-----------------------------FN_DYNVARS_165_04----------------#'
SET SESSION sql_select_limit = 2;
affected rows: 0
CREATE PROCEDURE TestProc()
BEGIN
SELECT * FROM t1;
END|
affected rows: 0
call TestProc();
a	b
1	val1
2	val2
3	val3
4	val4
5	val5
6	val6
7	val7
8	val8
9	val9
affected rows: 9
affected rows: 0
Expecting affected rows: 9
DROP PROCEDURE TestProc;
affected rows: 0
CREATE PROCEDURE TestProc()
BEGIN
SELECT * FROM t1 INNER JOIN t2 ON t1.a = t2.a;
END|
affected rows: 0
call TestProc();
a	b	a	b
5	val5	5	val5
6	val6	6	val6
7	val7	7	val7
8	val8	8	val8
9	val9	9	val9
affected rows: 5
affected rows: 0
Expecting affected rows: 5
DROP PROCEDURE TestProc;
affected rows: 0
'#-----------------------------FN_DYNVARS_165_05-----------------#'
SET SESSION sql_select_limit = 2;
affected rows: 0
CREATE TEMPORARY TABLE t4(a int PRIMARY KEY, b varchar(20)) SELECT * FROM t2;
affected rows: 5
info: Records: 5  Duplicates: 0  Warnings: 0
Expecting Records: 5
INSERT INTO t3 SELECT * FROM t1;
affected rows: 9
info: Records: 9  Duplicates: 0  Warnings: 0
Expecting Records: 9
'#-----------------------------FN_DYNVARS_165_06---------------------#'
SET GLOBAL sql_select_limit = 2;
** Connecting con_int2 using root **
** Connection con_int1 **
SELECT @@SESSION.sql_select_limit;
@@SESSION.sql_select_limit
2
2 Expected
SET SESSION sql_select_limit = 10;
** Connecting con_int2 using root **
** Connection con_int2 **
SELECT @@SESSION.sql_select_limit;
@@SESSION.sql_select_limit
2
2 Expected
SET SESSION sql_select_limit = 12;
** Connection con_int2 **
SELECT @@SESSION.sql_select_limit;
@@SESSION.sql_select_limit
12
12 Expected
** Connection con_int1 **
SELECT @@SESSION.sql_select_limit;
@@SESSION.sql_select_limit
10
10 Expected
SELECT @@GLOBAL.sql_select_limit;
@@GLOBAL.sql_select_limit
2
2 Expected
** Connection default **
Disconnecting Connections con_int1, con_int2
SET @@SESSION.sql_select_limit = @session_sql_select_limit;
SET @@GLOBAL.sql_select_limit = @global_sql_select_limit;
DROP TABLE t1;
DROP TABLE t2;
DROP TABLE t3;
DROP TABLE t4;
