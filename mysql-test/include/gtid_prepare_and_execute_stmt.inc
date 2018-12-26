# ==== Purpose ====
#
# Verify that GTID ownership is released for explicit prepared statement
# with specified GTID, which is added in 'gtid_executed' table.
#
# ==== Implementation ====
#
# 1. SET SESSION GTID_NEXT='UUID:GNO'
# 2. Prepare a statment
# 3. Execute the statement
# 4. See that an entry is added in 'gtid_executed' table.
#
# ==== Usage ====
#
# --let $statement= SQL command
# --source include/prepare_and_execute_stmt.test

--connection default
SET GTID_NEXT = 'AUTOMATIC';
--replace_result $master_uuid MASTER_UUID
--let $number = `SELECT GTID_NEXT_GENERATED(@@GLOBAL.GTID_EXECUTED, @@GLOBAL.SERVER_UUID)`
--eval SET SESSION GTID_NEXT='$master_uuid:$number'
--eval PREPARE stmt FROM '$statement'
EXECUTE stmt;

--connection connection_for_table_check
--let $gtid_step_check_table= 1
--source include/gtid_step_assert.inc
