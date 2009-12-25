** Setup **

SET @default_sql_log_off = @@sql_log_off;
CREATE TEMPORARY TABLE t1(a varchar(20), b varchar(20));
INSERT INTO t1 VALUES('aa1','bb');
INSERT INTO t1 VALUES('aa2','bb');
INSERT INTO t1 VALUES('aa3','bb');
INSERT INTO t1 VALUES('aa4','bb');
INSERT INTO t1 VALUES('aa5','bb');
'#--------------------FN_DYNVARS_158_01-------------------------#'
SET SESSION sql_log_off = ON;
UPDATE t1 SET a = 'aa1-updated' WHERE a = 'aa1';
Checking if log contains the executed statement
SELECT argument FROM mysql.general_log WHERE argument = 'UPDATE t1 SET a = \'aa1-updated\' WHERE a = \'aa1\'';
argument
'#--------------------FN_DYNVARS_158_02-------------------------#'
SET SESSION sql_log_off = OFF;
UPDATE t1 SET a = 'aa1' WHERE a = 'aa1-updated';
Checking if log contains the executed statement
SELECT argument FROM mysql.general_log WHERE argument = 'UPDATE t1 SET a = \'aa1\' WHERE a = \'aa1-updated\'';
argument
'#--------------------FN_DYNVARS_158_03--------------------------#'
** Connecting con_int1 using root **
** Connection con_int1 **
SELECT @@SESSION.sql_log_off;
@@SESSION.sql_log_off
0
0 / FALSE Expected
SET SESSION sql_log_off = FALSE;
** Connecting con_int2 using root **
** Connection con_int2 **
SELECT @@SESSION.sql_log_off;
@@SESSION.sql_log_off
0
0 / FALSE Expected
SET SESSION sql_log_off = TRUE;
** Connection con_int2 **
SELECT @@SESSION.sql_log_off;
@@SESSION.sql_log_off
1
1 / TRUE Expected
** Connection con_int1 **
SELECT @@SESSION.sql_log_off;
@@SESSION.sql_log_off
0
0 / FALSE Expected
** Connection default **
Disconnecting Connections con_int1, con_int2
SET SESSION sql_log_off = @default_sql_log_off;
DROP TABLE t1;
