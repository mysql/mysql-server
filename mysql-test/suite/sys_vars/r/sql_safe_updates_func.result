** Setup **

SET @default_sql_safe_updates = @@sql_safe_updates;
CREATE TEMPORARY TABLE t1(a int PRIMARY KEY, b varchar(20));
INSERT INTO t1 VALUES(1, 'val1');
INSERT INTO t1 VALUES(2, 'val2');
INSERT INTO t1 VALUES(3, 'val3');
INSERT INTO t1 VALUES(4, 'val4');
INSERT INTO t1 VALUES(5, 'val5');
INSERT INTO t1 VALUES(6, 'val6');
INSERT INTO t1 VALUES(7, 'val7');
INSERT INTO t1 VALUES(8, 'val8');
INSERT INTO t1 VALUES(9, 'val9');
'#-----------------------------FN_DYNVARS_164_01------------------------------------#'

Value ON

SET SESSION sql_safe_updates = ON;
'#-----------------------------FN_DYNVARS_164_02------------------------------------#'
Expected error : Update without key in safe mode
DELETE FROM t1;
ERROR HY000: You are using safe update mode and you tried to update a table without a WHERE that uses a KEY column. 
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
PREPARE stmt FROM 'DELETE FROM t1;';
Expected error : Update without key in safe mode
EXECUTE stmt;
ERROR HY000: You are using safe update mode and you tried to update a table without a WHERE that uses a KEY column. 
DEALLOCATE PREPARE stmt;
'#-----------------------------FN_DYNVARS_164_03------------------------------------#'
No error : Update without key in safe mode but with LIMIT
DELETE FROM t1 LIMIT 2;
SELECT * FROM t1;
a	b
3	val3
4	val4
5	val5
6	val6
7	val7
8	val8
9	val9
'#-----------------------------FN_DYNVARS_164_04------------------------------------#'
Expected error : Update without key in safe mode
DELETE FROM t1 WHERE b='val1';
ERROR HY000: You are using safe update mode and you tried to update a table without a WHERE that uses a KEY column. 
SELECT * FROM t1;
a	b
3	val3
4	val4
5	val5
6	val6
7	val7
8	val8
9	val9
'#-----------------------------FN_DYNVARS_164_05------------------------------------#'
This one should work
DELETE FROM t1 WHERE a=3;
SELECT * FROM t1;
a	b
4	val4
5	val5
6	val6
7	val7
8	val8
9	val9

Value OFF

SET SESSION sql_safe_updates = OFF;
'#-----------------------------FN_DYNVARS_164_06------------------------------------#'
This one should work
DELETE FROM t1 WHERE b='val1';
SELECT * FROM t1;
a	b
4	val4
5	val5
6	val6
7	val7
8	val8
9	val9
'#-----------------------------FN_DYNVARS_164_07------------------------------------#'
This one should work
DELETE FROM t1 WHERE a=5;
SELECT * FROM t1;
a	b
4	val4
6	val6
7	val7
8	val8
9	val9
'#-----------------------------FN_DYNVARS_164_08------------------------------------#'
This one should work
DELETE FROM t1 LIMIT 2;
SELECT * FROM t1;
a	b
7	val7
8	val8
9	val9
'#-----------------------------FN_DYNVARS_164_09------------------------------------#'
This one should work
DELETE FROM t1;
Bug#35392 Unexpected error occurs in this statement Can't change size of the file
statement is remarked because the error is uncatchable by testing framework
SELECT * FROM t1;
a	b
7	val7
8	val8
9	val9
PREPARE stmt FROM 'DELETE FROM t1;';
Expected error : Update without key in safe mode
Bug#35392 Unexpected error occurs in this statement Can't change size of the file
statement is remarked because the error is uncatchable by testing framework
DEALLOCATE PREPARE stmt;
'#----------------------------FN_DYNVARS_164_10--------------------------------------#'
** Connecting con_int1 using root **
** Connection con_int1 **
SELECT @@SESSION.sql_safe_updates;
@@SESSION.sql_safe_updates
0
0 / FALSE Expected
SET SESSION sql_safe_updates = FALSE;
** Connecting con_int2 using root **
** Connection con_int2 **
SELECT @@SESSION.sql_safe_updates;
@@SESSION.sql_safe_updates
0
0 / FALSE Expected
SET SESSION sql_safe_updates = TRUE;
** Connection con_int2 **
SELECT @@SESSION.sql_safe_updates;
@@SESSION.sql_safe_updates
1
1 / TRUE Expected
** Connection con_int1 **
SELECT @@SESSION.sql_safe_updates;
@@SESSION.sql_safe_updates
0
0 / FALSE Expected
** Connection default **
Disconnecting Connections con_int1, con_int2
SET SESSION sql_safe_updates = @default_sql_safe_updates;
DROP TABLE t1;
