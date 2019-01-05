#
# Try various things when InnoDB runs out of free undo logs.
#
# These first three injection points are during DDL_Log removal
# after a CREATE TABLE
# Inject error #1
SET GLOBAL DEBUG='+d,DDL_Log_remove_inject_error_1';
CREATE TABLE t1 (f1 INT, f2 CHAR(6), KEY(f2));
ERROR HY000: Too many active concurrent transactions
SET GLOBAL DEBUG='-d,DDL_Log_remove_inject_error_1';
# Try more DDL, restart, try more DDL
CREATE TABLE t9 (f1 INT, f2 CHAR(6), KEY(f2));
DROP TABLE t9;
# restart
CREATE TABLE t9 (f1 INT, f2 CHAR(6), KEY(f2));
DROP TABLE t9;
# Inject error #2
SET GLOBAL DEBUG='+d,DDL_Log_remove_inject_error_2';
CREATE TABLE t1 (f1 INT, f2 CHAR(6), KEY(f2));
ERROR HY000: Too many active concurrent transactions
SET GLOBAL DEBUG='-d,DDL_Log_remove_inject_error_2';
# Try more DDL, restart, try more DDL
CREATE TABLE t9 (f1 INT, f2 CHAR(6), KEY(f2));
DROP TABLE t9;
# restart
CREATE TABLE t9 (f1 INT, f2 CHAR(6), KEY(f2));
DROP TABLE t9;
# Inject error #3
SET GLOBAL DEBUG='+d,DDL_Log_remove_inject_error_3';
CREATE TABLE t1 (f1 INT, f2 CHAR(6), KEY(f2));
ERROR HY000: Too many active concurrent transactions
SET GLOBAL DEBUG='-d,DDL_Log_remove_inject_error_3';
# Try more DDL, restart, try more DDL
CREATE TABLE t9 (f1 INT, f2 CHAR(6), KEY(f2));
DROP TABLE t9;
# restart
CREATE TABLE t9 (f1 INT, f2 CHAR(6), KEY(f2));
DROP TABLE t9;
# These next two injection points are during DDL_Log removal
# after a RENAME TABLE
CREATE TABLE t1 (f1 INT, f2 CHAR(6), KEY(f2));
# Inject error #4
SET GLOBAL DEBUG='+d,DDL_Log_remove_inject_error_4';
ALTER TABLE t1 RENAME TO t2;
ERROR HY000: Error on rename of 'OLD_FILE_NAME' to 'NEW_FILE_NAME' (errno: 177 - Too many active concurrent transactions)
SET GLOBAL DEBUG='-d,DDL_Log_remove_inject_error_4';
ALTER TABLE t1 RENAME TO t2;
# Try more DDL, restart, try more DDL
CREATE TABLE t9 (f1 INT, f2 CHAR(6), KEY(f2));
DROP TABLE t9;
# restart
CREATE TABLE t9 (f1 INT, f2 CHAR(6), KEY(f2));
DROP TABLE t9;
# Inject error #5
SET GLOBAL DEBUG='+d,DDL_Log_remove_inject_error_5';
ALTER TABLE t2 RENAME TO t1;
ERROR HY000: Error on rename of 'OLD_FILE_NAME' to 'NEW_FILE_NAME' (errno: 177 - Too many active concurrent transactions)
SET GLOBAL DEBUG='-d,DDL_Log_remove_inject_error_5';
ALTER TABLE t2 RENAME TO t1;
DROP TABLE t1;
# Try more DDL, restart, try more DDL
CREATE TABLE t9 (f1 INT, f2 CHAR(6), KEY(f2));
DROP TABLE t9;
# restart
CREATE TABLE t9 (f1 INT, f2 CHAR(6), KEY(f2));
DROP TABLE t9;
