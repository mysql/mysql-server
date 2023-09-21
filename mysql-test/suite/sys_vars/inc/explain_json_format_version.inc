SET @global_start_val = @@global.explain_json_format_version;
SET @session_start_val = @@session.explain_json_format_version;

SELECT @global_start_val, @@global.explain_json_format_version, @session_start_val, @@session.explain_json_format_version;

--echo # GLOBAL assignment

SET @@global.explain_json_format_version = DEFAULT;
SELECT @@global.explain_json_format_version;

SET GLOBAL explain_json_format_version = DEFAULT;
SELECT @@global.explain_json_format_version;

SET @@global.explain_json_format_version = 1;
SELECT @@global.explain_json_format_version;
SET @@global.explain_json_format_version = 2;
SELECT @@global.explain_json_format_version;

--echo # Warnings for out of bounds values
SET @@global.explain_json_format_version = 0;
SELECT @@global.explain_json_format_version;
SET @@global.explain_json_format_version = 3;
SELECT @@global.explain_json_format_version;

SET @@global.explain_json_format_version = DEFAULT;
SELECT @@global.explain_json_format_version;

--echo # Errors for invalid values
--error ER_WRONG_TYPE_FOR_VAR
SET @@global.explain_json_format_version = '1';
--error ER_WRONG_TYPE_FOR_VAR
SET @@global.explain_json_format_version = '2';
--error ER_WRONG_TYPE_FOR_VAR
SET @@global.explain_json_format_version = 'NO';
--error ER_WRONG_TYPE_FOR_VAR
SET @@global.explain_json_format_version = 'YES';
SELECT @@global.explain_json_format_version;

--disable_query_log
--disable_warnings
SET @saved_sql_mode = @@sql_mode;
SET sql_mode = 'STRICT_ALL_TABLES';
--enable_warnings
--enable_query_log

--echo # Errors for invalid values in strict mode
--error ER_WRONG_VALUE_FOR_VAR
SET @@global.explain_json_format_version = 0;
--error ER_WRONG_VALUE_FOR_VAR
SET @@global.explain_json_format_version = 3;
--error ER_WRONG_TYPE_FOR_VAR
SET @@global.explain_json_format_version = '1';
--error ER_WRONG_TYPE_FOR_VAR
SET @@global.explain_json_format_version = '2';
--error ER_WRONG_TYPE_FOR_VAR
SET @@global.explain_json_format_version = 'NO';
--error ER_WRONG_TYPE_FOR_VAR
SET @@global.explain_json_format_version = 'YES';
SELECT @@global.explain_json_format_version;

--disable_query_log
SET sql_mode = @saved_sql_mode;
--enable_query_log

SET @@global.explain_json_format_version = @global_start_val;

--echo
--echo # SESSION assignment

SET @@session.explain_json_format_version = DEFAULT;
SELECT @@session.explain_json_format_version;

SET SESSION explain_json_format_version = DEFAULT;
SELECT @@session.explain_json_format_version;

SET explain_json_format_version = DEFAULT;
SELECT @@session.explain_json_format_version;

SET @@session.explain_json_format_version = 1;
SELECT @@session.explain_json_format_version;
SET @@session.explain_json_format_version = 2;
SELECT @@session.explain_json_format_version;

--echo # Warnings for out of bounds values
SET @@session.explain_json_format_version = 0;
SELECT @@session.explain_json_format_version;
SET @@session.explain_json_format_version = 3;
SELECT @@session.explain_json_format_version;

SET @@session.explain_json_format_version = DEFAULT;
SELECT @@session.explain_json_format_version;

--echo # Errors for invalid values
--error ER_WRONG_TYPE_FOR_VAR
SET @@session.explain_json_format_version = '1';
--error ER_WRONG_TYPE_FOR_VAR
SET @@session.explain_json_format_version = '2';
--error ER_WRONG_TYPE_FOR_VAR
SET @@session.explain_json_format_version = 'NO';
--error ER_WRONG_TYPE_FOR_VAR
SET @@session.explain_json_format_version = 'YES';
SELECT @@session.explain_json_format_version;

--disable_query_log
--disable_warnings
SET @saved_sql_mode = @@sql_mode;
SET sql_mode = 'STRICT_ALL_TABLES';
--enable_warnings
--enable_query_log

--echo # Errors for invalid values in strict mode
--error ER_WRONG_VALUE_FOR_VAR
SET @@session.explain_json_format_version = 0;
--error ER_WRONG_VALUE_FOR_VAR
SET @@session.explain_json_format_version = 3;
--error ER_WRONG_TYPE_FOR_VAR
SET @@session.explain_json_format_version = '1';
--error ER_WRONG_TYPE_FOR_VAR
SET @@session.explain_json_format_version = '2';
--error ER_WRONG_TYPE_FOR_VAR
SET @@session.explain_json_format_version = 'NO';
--error ER_WRONG_TYPE_FOR_VAR
SET @@session.explain_json_format_version = 'YES';
SELECT @@session.explain_json_format_version;

--disable_query_log
SET sql_mode = @saved_sql_mode;
--enable_query_log

SET @@session.explain_json_format_version = @session_start_val;
