##
## Test the Performance Schema-based implementation of SHOW PROCESSLIST.
## Verify behavior for regular users and PROCESS_ACL.
##
SELECT @@global.performance_schema_show_processlist INTO @save_processlist;
SET @@global.performance_schema_show_processlist = OFF;
CREATE USER 'regular'@'localhost';
SHOW GRANTS;
Grants for regular@localhost
GRANT USAGE ON *.* TO `regular`@`localhost`
SELECT USER, INFO FROM INFORMATION_SCHEMA.PROCESSLIST;
USER	INFO
regular	SELECT USER, INFO FROM INFORMATION_SCHEMA.PROCESSLIST
SELECT USER, INFO FROM performance_schema.processlist;
USER	INFO
regular	SELECT USER, INFO FROM performance_schema.processlist
SHOW PROCESSLIST;
SELECT "Previous statement is now completed." as status;
status
Previous statement is now completed.
SELECT EVENT_NAME, SQL_TEXT, ROWS_SENT as BROKEN_ROWS_SENT
FROM performance_schema.events_statements_history
WHERE SQL_TEXT = "SHOW PROCESSLIST";
EVENT_NAME	SQL_TEXT	BROKEN_ROWS_SENT
statement/sql/show_processlist	SHOW PROCESSLIST	0
TRUNCATE TABLE performance_schema.events_statements_history;
SET @@global.performance_schema_show_processlist = ON;
SHOW GRANTS;
Grants for regular@localhost
GRANT USAGE ON *.* TO `regular`@`localhost`
SELECT USER, INFO FROM INFORMATION_SCHEMA.PROCESSLIST;
USER	INFO
regular	SELECT USER, INFO FROM INFORMATION_SCHEMA.PROCESSLIST
SELECT USER, INFO FROM performance_schema.processlist;
USER	INFO
regular	SELECT USER, INFO FROM performance_schema.processlist
SHOW PROCESSLIST;
SELECT "Previous statement is now completed." as status;
status
Previous statement is now completed.
SELECT EVENT_NAME, SQL_TEXT, ROWS_SENT
FROM performance_schema.events_statements_history
WHERE SQL_TEXT = "SHOW PROCESSLIST";
EVENT_NAME	SQL_TEXT	ROWS_SENT
statement/sql/show_processlist	SHOW PROCESSLIST	1
TRUNCATE TABLE performance_schema.events_statements_history;
GRANT PROCESS ON *.* TO 'regular'@'localhost';
SET @@global.performance_schema_show_processlist = OFF;
SHOW GRANTS;
Grants for regular@localhost
GRANT PROCESS ON *.* TO `regular`@`localhost`
SELECT count(*) >= 2 FROM INFORMATION_SCHEMA.PROCESSLIST;
count(*) >= 2
1
SELECT count(*) >= 2 FROM performance_schema.processlist;
count(*) >= 2
1
SHOW PROCESSLIST;
SELECT "Previous statement is now completed." as status;
status
Previous statement is now completed.
SELECT EVENT_NAME, SQL_TEXT, ROWS_SENT as BROKEN_ROWS_SENT
FROM performance_schema.events_statements_history
WHERE SQL_TEXT = "SHOW PROCESSLIST";
EVENT_NAME	SQL_TEXT	BROKEN_ROWS_SENT
statement/sql/show_processlist	SHOW PROCESSLIST	0
TRUNCATE TABLE performance_schema.events_statements_history;
SET @@global.performance_schema_show_processlist = ON;
SHOW GRANTS;
Grants for regular@localhost
GRANT PROCESS ON *.* TO `regular`@`localhost`
SELECT count(*) >= 2 FROM INFORMATION_SCHEMA.PROCESSLIST;
count(*) >= 2
1
SELECT count(*) >= 2 FROM performance_schema.processlist;
count(*) >= 2
1
SHOW PROCESSLIST;
SELECT "Previous statement is now completed." as status;
status
Previous statement is now completed.
SELECT EVENT_NAME, SQL_TEXT, ROWS_SENT >= 2
FROM performance_schema.events_statements_history
WHERE SQL_TEXT = "SHOW PROCESSLIST";
EVENT_NAME	SQL_TEXT	ROWS_SENT >= 2
statement/sql/show_processlist	SHOW PROCESSLIST	1
TRUNCATE TABLE performance_schema.events_statements_history;
SET @@global.performance_schema_show_processlist = @save_processlist;
DROP USER 'regular'@'localhost';
