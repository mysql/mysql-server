##
## Test the Performance Schema-based implementation of SHOW PROCESSLIST.
## Verify behavior for anonymous users and PROCESS_ACL.
##
SELECT @@global.performance_schema_show_processlist INTO @save_processlist;
SET @@global.performance_schema_show_processlist = OFF;
SHOW GRANTS;
Grants for @localhost
GRANT USAGE ON *.* TO ``@`localhost`
SELECT * FROM INFORMATION_SCHEMA.PROCESSLIST;
ID	USER	HOST	DB	COMMAND	TIME	STATE	INFO
SELECT * FROM performance_schema.processlist;
ID	USER	HOST	DB	COMMAND	TIME	STATE	INFO	EXECUTION_ENGINE
SHOW PROCESSLIST;
ERROR 42000: Access denied; you need (at least one of) the PROCESS privilege(s) for this operation
SET @@global.performance_schema_show_processlist = ON;
SHOW GRANTS;
Grants for @localhost
GRANT USAGE ON *.* TO ``@`localhost`
SELECT * FROM INFORMATION_SCHEMA.PROCESSLIST;
ID	USER	HOST	DB	COMMAND	TIME	STATE	INFO
SELECT * FROM performance_schema.processlist;
ID	USER	HOST	DB	COMMAND	TIME	STATE	INFO	EXECUTION_ENGINE
SHOW PROCESSLIST;
ERROR 42000: Access denied; you need (at least one of) the PROCESS privilege(s) for this operation
GRANT PROCESS ON *.* TO ''@'localhost';
SET @@global.performance_schema_show_processlist = OFF;
SHOW GRANTS;
Grants for @localhost
GRANT PROCESS ON *.* TO ``@`localhost`
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
set @@global.performance_schema_show_processlist = ON;
SHOW GRANTS;
Grants for @localhost
GRANT PROCESS ON *.* TO ``@`localhost`
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
SET @@global.performance_schema_show_processlist = @save_processlist;
