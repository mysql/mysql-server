** Setup **
SET @global_automatic_sp_privileges = @@GLOBAL.automatic_sp_privileges;
CREATE TABLE t1 (a varchar(200));
INSERT INTO t1 VALUES('Procedure Executed.');
CREATE USER 'userTest'@'localhost';
CREATE USER 'userTest1'@'localhost';
** Connection default **
SET GLOBAL automatic_sp_privileges = TRUE;
** Connecting using userTest **
** Connection conUser **
CREATE PROCEDURE testProc ()
BEGIN
SELECT * FROM t1;
END;|
CALL testProc();
a
Procedure Executed.
Expecting SELECT executed
** Connection default**
SET GLOBAL automatic_sp_privileges = FALSE;
** Connecting using userTest1 **
** Connection conUser1 **
CREATE PROCEDURE testProc1 ()
BEGIN
SELECT * FROM t1;
END;|

Expected error access denied
CALL testProc1();
ERROR 42000: execute command denied to user 'userTest1'@'localhost' for routine 'test.testProc1'

Expected error access denied
ALTER PROCEDURE testProc1 COMMENT 'My Comment';
ERROR 42000: alter routine command denied to user 'userTest1'@'localhost' for routine 'test.testProc1'

Expected error access denied
DROP PROCEDURE testProc1;
ERROR 42000: alter routine command denied to user 'userTest1'@'localhost' for routine 'test.testProc1'
** Connection default **
GRANT EXECUTE, ALTER ROUTINE ON PROCEDURE testProc1 TO 'userTest1'@'localhost';
** Connection conUser1 **
CALL testProc1();
a
Procedure Executed.
Expecting seelect executed
ALTER PROCEDURE testProc1 COMMENT 'My Comment';

** Cleanup **
** Connection default **
disconnecting connections
SET GLOBAL automatic_sp_privileges = @global_automatic_sp_privileges;
REVOKE EXECUTE, ALTER ROUTINE ON PROCEDURE testProc FROM 'userTest'@'localhost';
REVOKE ALL PRIVILEGES, GRANT OPTION FROM 'userTest'@'localhost';
REVOKE ALL PRIVILEGES, GRANT OPTION FROM 'userTest1'@'localhost';
DROP PROCEDURE testProc;
DROP PROCEDURE testProc1;
DROP USER 'userTest'@'localhost';
DROP USER 'userTest1'@'localhost';
DROP TABLE t1;
