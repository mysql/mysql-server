** Setup **

** Connecting con0 using root **
** Connecting con1 using root **
** Connection default **
SET @global_low_priority_updates = @@GLOBAL.low_priority_updates;
SET @session_low_priority_updates = @@SESSION.low_priority_updates;
CREATE TABLE t1 (a varchar(100));
create view v1 as select * from t1;
'#--------------------FN_DYNVARS_160_01-------------------------#'
** Connection con0 **
SET SESSION low_priority_updates = ON;
** Connection con1 **
SET SESSION low_priority_updates = ON;
** Connection default **
SET SESSION low_priority_updates = ON;
INSERT INTO t1 VALUES('1');
INSERT INTO t1 VALUES('2');
INSERT INTO t1 VALUES('3');
INSERT INTO t1 VALUES('4');
INSERT INTO t1 VALUES('5');
INSERT INTO t1 VALUES('6');
LOCK TABLE v1 WRITE;
** Connection con1 **
** Asynchronous Execution **
UPDATE t1 SET a = CONCAT(a,"-updated");|
** Connection con0 **
** Asynchronous Execution **
LOCK TABLE t1 READ;
SELECT * FROM t1;
UNLOCK TABLES;|
** Connection default **
UNLOCK TABLES;
** Connection con0 **
** Asynchronous Result **
a
1
2
3
4
5
6
Expected values of a without -updated;
** Connection default **
DELETE FROM t1;
'#--------------------FN_DYNVARS_160_02-------------------------#'
** Connection con0 **
SET SESSION low_priority_updates = OFF;
** Connection con1 **
SET SESSION low_priority_updates = OFF;
** Connection default**
SET SESSION low_priority_updates = OFF;
INSERT INTO t1 VALUES('1');
INSERT INTO t1 VALUES('2');
INSERT INTO t1 VALUES('3');
INSERT INTO t1 VALUES('4');
INSERT INTO t1 VALUES('5');
INSERT INTO t1 VALUES('6');
LOCK TABLE v1 WRITE;
** Connection con1 **
** Asynchronous Execution **
UPDATE t1 SET a = CONCAT(a,"-updated");|
** Connection con0 **
** Asynchronous Execution **
LOCK TABLE t1 READ;
SELECT * FROM t1 ORDER BY a ASC;
UNLOCK TABLES;|
** Connection default **
UNLOCK TABLES;
** Connection con0 **
** Asynchronous Result **
a
1-updated
2-updated
3-updated
4-updated
5-updated
6-updated
Expected values of a with -updated;
** Connection default**
DELETE FROM t1;
'#--------------------FN_DYNVARS_160_03-------------------------#'
** Connecting con_int1 using root **
** Connection con_int1 **
SELECT @@SESSION.low_priority_updates;
@@SESSION.low_priority_updates
0
1 / TRUE Expected
SET SESSION low_priority_updates = FALSE;
** Connecting con_int2 using root **
** Connection con_int2 **
SELECT @@SESSION.low_priority_updates;
@@SESSION.low_priority_updates
0
1 / TRUE Expected
SET SESSION low_priority_updates = TRUE;
** Connection con_int1 **
SELECT @@SESSION.low_priority_updates;
@@SESSION.low_priority_updates
0
0 / FALSE Expected
** Connection con_int2 **
SELECT @@SESSION.low_priority_updates;
@@SESSION.low_priority_updates
1
1 / TRUE Expected
** Connection default  **
Disconnecting Connections con_int1, con_int2
** Connection default **
Disconnecting Connections con0, con1
drop view v1;
DROP TABLE t1;
SET @@GLOBAL.low_priority_updates = @global_low_priority_updates;
SET @@SESSION.low_priority_updates = @session_low_priority_updates;
