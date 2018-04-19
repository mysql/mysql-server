SHOW GLOBAL VARIABLES LIKE "performance_schema_error_size";
Variable_name	Value
performance_schema_error_size	0
TRUNCATE TABLE performance_schema.events_errors_summary_global_by_error;
TRUNCATE TABLE performance_schema.events_errors_summary_by_thread_by_error;
TRUNCATE TABLE performance_schema.events_errors_summary_by_user_by_error;
TRUNCATE TABLE performance_schema.events_errors_summary_by_host_by_error;
TRUNCATE TABLE performance_schema.events_errors_summary_by_account_by_error;
USE no_db;
ERROR 42000: Unknown database 'no_db'
SELECT * FROM no_table;
ERROR 42S02: Table 'test.no_table' doesn't exist
####################################
# Fetching Errors' Stastics
####################################
SELECT * FROM performance_schema.events_errors_summary_global_by_error WHERE SUM_ERROR_HANDLED>0 OR SUM_ERROR_RAISED>0;
ERROR_NUMBER	ERROR_NAME	SQL_STATE	SUM_ERROR_RAISED	SUM_ERROR_HANDLED	FIRST_SEEN	LAST_SEEN
SELECT * FROM performance_schema.events_errors_summary_by_thread_by_error WHERE SUM_ERROR_HANDLED>0 OR SUM_ERROR_RAISED>0;
THREAD_ID	ERROR_NUMBER	ERROR_NAME	SQL_STATE	SUM_ERROR_RAISED	SUM_ERROR_HANDLED	FIRST_SEEN	LAST_SEEN
SELECT * FROM performance_schema.events_errors_summary_by_user_by_error WHERE SUM_ERROR_HANDLED>0 OR SUM_ERROR_RAISED>0;
USER	ERROR_NUMBER	ERROR_NAME	SQL_STATE	SUM_ERROR_RAISED	SUM_ERROR_HANDLED	FIRST_SEEN	LAST_SEEN
SELECT * FROM performance_schema.events_errors_summary_by_host_by_error WHERE SUM_ERROR_HANDLED>0 OR SUM_ERROR_RAISED>0;
HOST	ERROR_NUMBER	ERROR_NAME	SQL_STATE	SUM_ERROR_RAISED	SUM_ERROR_HANDLED	FIRST_SEEN	LAST_SEEN
SELECT * FROM performance_schema.events_errors_summary_by_account_by_error WHERE SUM_ERROR_HANDLED>0 OR SUM_ERROR_RAISED>0;
USER	HOST	ERROR_NUMBER	ERROR_NAME	SQL_STATE	SUM_ERROR_RAISED	SUM_ERROR_HANDLED	FIRST_SEEN	LAST_SEEN
