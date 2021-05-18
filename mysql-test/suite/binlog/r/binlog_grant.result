reset master;
set @saved_binlog_format = @@global.binlog_format;
create user mysqltest_1@localhost;
show grants for mysqltest_1@localhost;
Grants for mysqltest_1@localhost
GRANT USAGE ON *.* TO `mysqltest_1`@`localhost`
**** Variable SQL_LOG_BIN ****
[root]
set session sql_log_bin = 1;
[plain]
set session sql_log_bin = 1;
ERROR 42000: Access denied; you need (at least one of) the SUPER, SYSTEM_VARIABLES_ADMIN or SESSION_VARIABLES_ADMIN privilege(s) for this operation
**** Variable BINLOG_FORMAT ****
[root]
set global binlog_format = 'row';
set session binlog_format = 'row';
[plain]
set global binlog_format = 'row';
ERROR 42000: Access denied; you need (at least one of) the SUPER or SYSTEM_VARIABLES_ADMIN privilege(s) for this operation
set session binlog_format = 'row';
ERROR 42000: Access denied; you need (at least one of) the SUPER, SYSTEM_VARIABLES_ADMIN or SESSION_VARIABLES_ADMIN privilege(s) for this operation
**** Clean up ****
set global binlog_format = @saved_binlog_format;
drop user mysqltest_1@localhost;
CREATE USER 'mysqltest_1'@'localhost';
GRANT REPLICATION CLIENT ON *.* TO 'mysqltest_1'@'localhost';
SHOW MASTER LOGS;
SHOW BINARY LOGS;
DROP USER 'mysqltest_1'@'localhost';
