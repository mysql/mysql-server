'#--------------------FN_DYNVARS_048_01-------------------------#'
SET @start_value= @@global.innodb_table_locks;
SELECT @start_value;
@start_value
1
SET @@global.innodb_table_locks = OFF;
'connect (con1,localhost,root,,,,)'
'connection con1'
SELECT @@global.innodb_table_locks;
@@global.innodb_table_locks
0
SELECT @@session.innodb_table_locks;
@@session.innodb_table_locks
0
'#--------------------FN_DYNVARS_048_02-------------------------#'
'----check when innodb_table_locks = ON and autocommit = OFF---'
'connect (con2,localhost,root,,,,)'
'connection default'
DROP TABLE IF EXISTS t1;
CREATE TABLE t1 (a INT) ENGINE=INNODB;
SET @@autocommit = OFF;
SET @@innodb_table_locks = ON;
BEGIN;
INSERT INTO t1 VALUES(1);
SELECT * FROM t1 FOR UPDATE;
a
1
'CONNECTION con2'
SET @@innodb_table_locks = ON;
SET @@autocommit = OFF;
LOCK TABLES t1 WRITE;
'CONNECTION default'
COMMIT;
'CONNECTION con2'
UNLOCK tables;
DROP TABLE t1;
SET @@global.innodb_table_locks= @start_value;
SELECT @@global.innodb_table_locks;
@@global.innodb_table_locks
1
