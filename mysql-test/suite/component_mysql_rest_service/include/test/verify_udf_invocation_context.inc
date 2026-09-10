# Verify invocation contexts for $verify_udf_name.
# $verify_udf_expected_state is the initial lifecycle state.
# Requires table mrs_lifecycle_context; restores READY before returning.

--echo # $verify_udf_name: reject non-SELECT commands without changing lifecycle state
--error ER_MRS_COMPONENT_UDF_ERROR
--eval DO $verify_udf_name()
--let $assert_text = rejected DO preserves lifecycle state
--let $assert_cond = "[SHOW GLOBAL STATUS LIKE "mysql_rest_service.state", Value, 1]" = "$verify_udf_expected_state"
--source include/assert.inc

--error ER_MRS_COMPONENT_UDF_ERROR
--eval SET @mrs_lifecycle_result = $verify_udf_name()
--let $assert_text = rejected SET preserves lifecycle state
--let $assert_cond = "[SHOW GLOBAL STATUS LIKE "mysql_rest_service.state", Value, 1]" = "$verify_udf_expected_state"
--source include/assert.inc

--error ER_MRS_COMPONENT_UDF_ERROR
--eval INSERT INTO mrs_lifecycle_context SELECT $verify_udf_name()
--let $assert_text = rejected INSERT SELECT preserves lifecycle state
--let $assert_cond = "[SHOW GLOBAL STATUS LIKE "mysql_rest_service.state", Value, 1]" = "$verify_udf_expected_state"
--source include/assert.inc

--echo # $verify_udf_name: check the invocation context at execution, not preparation
--eval PREPARE mrs_lifecycle_stmt FROM 'DO $verify_udf_name()'
--error ER_MRS_COMPONENT_UDF_ERROR
EXECUTE mrs_lifecycle_stmt;
DEALLOCATE PREPARE mrs_lifecycle_stmt;
--let $assert_text = rejected prepared DO preserves lifecycle state
--let $assert_cond = "[SHOW GLOBAL STATUS LIKE "mysql_rest_service.state", Value, 1]" = "$verify_udf_expected_state"
--source include/assert.inc

# SELECT within these bodies ensures that the substatement check, rather
# than the SQL-command check, rejects the invocation.
delimiter |;
eval CREATE FUNCTION mrs_lifecycle_function() RETURNS INTEGER READS SQL DATA
BEGIN
  DECLARE result INTEGER;
  SELECT $verify_udf_name() INTO result;
  RETURN result;
END|
eval CREATE TRIGGER mrs_lifecycle_trigger
BEFORE INSERT ON mrs_lifecycle_context FOR EACH ROW
  SELECT $verify_udf_name() INTO @mrs_lifecycle_result|
delimiter ;|

--echo # $verify_udf_name: reject SELECT inside stored functions and triggers
--error ER_MRS_COMPONENT_UDF_ERROR
SELECT mrs_lifecycle_function();
--let $assert_text = rejected stored function preserves lifecycle state
--let $assert_cond = "[SHOW GLOBAL STATUS LIKE "mysql_rest_service.state", Value, 1]" = "$verify_udf_expected_state"
--source include/assert.inc

--error ER_MRS_COMPONENT_UDF_ERROR
INSERT INTO mrs_lifecycle_context VALUES (0);
--let $assert_text = rejected trigger preserves lifecycle state
--let $assert_cond = "[SHOW GLOBAL STATUS LIKE "mysql_rest_service.state", Value, 1]" = "$verify_udf_expected_state"
--source include/assert.inc
SELECT COUNT(*) FROM mrs_lifecycle_context;

DROP TRIGGER mrs_lifecycle_trigger;
DROP FUNCTION mrs_lifecycle_function;

--echo # $verify_udf_name: allow direct and prepared SELECT
--eval SELECT $verify_udf_name()
--eval PREPARE mrs_lifecycle_stmt FROM 'SELECT $verify_udf_name()'
EXECUTE mrs_lifecycle_stmt;
DEALLOCATE PREPARE mrs_lifecycle_stmt;

--echo # $verify_udf_name: allow SELECT inside a directly invoked stored procedure
--eval CREATE PROCEDURE mrs_lifecycle_procedure() SELECT $verify_udf_name() INTO @mrs_lifecycle_result
CALL mrs_lifecycle_procedure();
SELECT @mrs_lifecycle_result;

# The same procedure must be rejected when called from a stored function.
delimiter |;
CREATE FUNCTION mrs_lifecycle_nested_function() RETURNS INTEGER READS SQL DATA
BEGIN
  CALL mrs_lifecycle_procedure();
  RETURN @mrs_lifecycle_result;
END|
delimiter ;|

if ($verify_udf_name != "mysql_rest_service_stop")
{
  --let $wait_condition=SELECT VARIABLE_VALUE = 'READY' FROM performance_schema.global_status WHERE VARIABLE_NAME = 'mysql_rest_service.state'
  --source include/wait_condition.inc
}
--let $verify_udf_expected_state = READY
if ($verify_udf_name == "mysql_rest_service_stop")
{
  --let $verify_udf_expected_state = STOPPED
}
--error ER_MRS_COMPONENT_UDF_ERROR
SELECT mrs_lifecycle_nested_function();
--let $assert_text = rejected nested stored procedure preserves lifecycle state
--let $assert_cond = "[SHOW GLOBAL STATUS LIKE "mysql_rest_service.state", Value, 1]" = "$verify_udf_expected_state"
--source include/assert.inc
DROP FUNCTION mrs_lifecycle_nested_function;
DROP PROCEDURE mrs_lifecycle_procedure;

SELECT mysql_rest_service_start();
--source ../cmrs/wait_mrs_read_metadata.inc
--let $wait_condition=SELECT VARIABLE_VALUE = 'READY' FROM performance_schema.global_status WHERE VARIABLE_NAME = 'mysql_rest_service.state'
--source include/wait_condition.inc
