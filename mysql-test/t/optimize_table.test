--echo #
--echo # WL#15819 - New privilege to control OPTIMIZE LOCAL TABLE Operation
--echo #

CREATE USER 'user01'@'localhost' IDENTIFIED BY '';
CREATE DATABASE test01;
CREATE TABLE test01.tbl01 (id int primary key, a varchar(100));
INSERT INTO test01.tbl01 SET id = 1, a = "xyz";

--echo # Ensure root has OPTIMIZE_LOCAL_TABLE priv by defualt
SELECT COUNT(*)=1 FROM mysql.global_grants WHERE USER='root' AND priv='OPTIMIZE_LOCAL_TABLE';

--echo # Root user executing optimize local|NO_WRITE_TO_BINLOG table statement.
OPTIMIZE LOCAL TABLE test01.tbl01;
OPTIMIZE NO_WRITE_TO_BINLOG TABLE test01.tbl01;

--echo # Root user executing optimize table statement.
OPTIMIZE TABLE test01.tbl01;

--enable_connect_log

connect (con1, localhost, user01,,);

--echo # user01 without OPTIMIZE_LOCAL_TABLE privilege executing 
--echo # optimize local|NO_WRITE_TO_BINLOG table statement. An error is reported.
--error ER_SPECIFIC_ACCESS_DENIED_ERROR
OPTIMIZE LOCAL TABLE test01.tbl01;
--error ER_SPECIFIC_ACCESS_DENIED_ERROR
OPTIMIZE NO_WRITE_TO_BINLOG TABLE test01.tbl01;

--echo # user01 without INSERT & SELECT privilege executing OPTIMIZE TABLE statement.
--echo # An error is reported.
--error ER_TABLEACCESS_DENIED_ERROR
OPTIMIZE TABLE test01.tbl01;

connection default;

--echo # Error is reported if the dynamic privilege is not granted at global level. 
--error ER_ILLEGAL_PRIVILEGE_LEVEL
GRANT OPTIMIZE_LOCAL_TABLE on test01.tbl01 TO 'user01'@'localhost';
--error ER_ILLEGAL_PRIVILEGE_LEVEL
GRANT OPTIMIZE_LOCAL_TABLE on test01.* TO 'user01'@'localhost';

--echo # user01 is granted OPTIMIZE_LOCAL_TABLE privilege.
GRANT OPTIMIZE_LOCAL_TABLE on *.* TO 'user01'@'localhost';
SELECT COUNT(*)=1 FROM mysql.global_grants WHERE USER='user01' AND priv='OPTIMIZE_LOCAL_TABLE';
SELECT COUNT(*)=1 from INFORMATION_SCHEMA.USER_PRIVILEGES WHERE GRANTEE="'user01'@'localhost'" and PRIVILEGE_TYPE ="OPTIMIZE_LOCAL_TABLE";

connection con1;
--echo # optimize local|NO_WRITE_TO_BINLOG table executed by user01 is successful 
OPTIMIZE LOCAL TABLE test01.tbl01;
OPTIMIZE NO_WRITE_TO_BINLOG TABLE test01.tbl01;

--echo # user01 does not have INSERT and SELECT privilege.
--echo # Hence optimize table operation fails.
--error ER_TABLEACCESS_DENIED_ERROR
OPTIMIZE TABLE test01.tbl01;

connection default;

--echo # Grant INSERT and SELECT for optimize table operation.
GRANT INSERT, SELECT on test01.tbl01 TO 'user01'@'localhost';

connection con1;
--echo # optimize table executed by user01 is successful
OPTIMIZE TABLE test01.tbl01;

connection default;
CREATE USER 'user02'@'localhost' IDENTIFIED BY '';
--echo # Insert the OPTIMIZE_LOCAL_TABLE privilege in global_grants and check if user has the priv.
INSERT INTO mysql.global_grants VALUES('user02', 'localhost', 'OPTIMIZE_LOCAL_TABLE', 'Y');
FLUSH PRIVILEGES;
SHOW GRANTS FOR 'user02'@'localhost';

--echo # Clean up
disconnect con1;
DROP DATABASE test01;
DROP USER 'user01'@'localhost';
DROP USER 'user02'@'localhost';
--disable_connect_log
