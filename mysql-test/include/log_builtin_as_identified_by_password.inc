--connection master
SHOW GLOBAL VARIABLES LIKE 'log_builtin_as_identified_by_password';

--echo #
--echo # Bug#21516392 : PREVENT ACCOUNT MANAGEMENT STATEMENTS FROM
--echo #                INJECTING ACCOUNT ATTRIBUTES
--echo # Bug#20535561 : CREATE USER IN BINLOG WITH
--echo #                LOG_BACKWARD_COMPATIBLE_USER_DEFINITIONS ON
--echo #

--echo [On Master]
CREATE USER u1@localhost PASSWORD EXPIRE NEVER;
CREATE USER u2@localhost ACCOUNT LOCK;
CREATE USER u3@localhost;
CREATE USER u4@localhost IDENTIFIED WITH 'mysql_native_password';
CREATE USER u5@localhost IDENTIFIED WITH 'mysql_native_password' AS '*67092806AE91BFB6BE72DE6C7BE2B7CCA8CFA9DF';
CREATE USER u6@localhost IDENTIFIED BY 'abcd';
CREATE USER u7@localhost IDENTIFIED WITH 'mysql_native_password' by 'abcd';

ALTER USER u1@localhost IDENTIFIED BY 'abcd';
ALTER USER u2@localhost ACCOUNT LOCK;
ALTER USER u3@localhost PASSWORD EXPIRE NEVER;
ALTER USER u4@localhost IDENTIFIED WITH 'mysql_native_password' AS '*67092806AE91BFB6BE72DE6C7BE2B7CCA8CFA9DF';
ALTER USER u1@localhost PASSWORD EXPIRE;
ALTER USER u2@localhost IDENTIFIED BY 'abcd', u3@localhost PASSWORD EXPIRE;
ALTER USER u4@localhost IDENTIFIED BY 'abcd', u5@localhost ACCOUNT LOCK;
ALTER USER u5@localhost, u6@localhost PASSWORD EXPIRE ACCOUNT LOCK;

DROP USER u1@localhost;
DROP USER u2@localhost;
DROP USER u3@localhost;
DROP USER u4@localhost;
DROP USER u5@localhost;
DROP USER u6@localhost;
DROP USER u7@localhost;

--source include/show_binlog_events.inc

--source include/sync_slave_sql_with_master.inc
--echo [On Slave]
--source include/show_binlog_events.inc
--source include/rpl_end.inc

--echo
--echo End of 5.7 tests!
--echo
