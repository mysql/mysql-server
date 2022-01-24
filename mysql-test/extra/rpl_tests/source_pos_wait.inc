# ==== Purpose ====
#
# Check requirements and regressions related to SOURCE_POS_WAIT.
#
# ==== Requirements ====
#
# R1. SOURCE_POS_WAIT should timeout as required
# R2. SOURCE_POS_WAIT should return an error if the replication applier
#     thread is stopped while the function is waiting.
# R3. SOURCE_POS_WAIT should return NULL when replication is not configured.
# R4. When negative timeout is given, SOURCE_POS_WAIT should return
#     error if sql_mode contains STRICT_ALL_TABLES; warning otherwise.
# R5. SOURCE_POS_WAIT should allow fractional seconds for the timeout;
#     it should not truncate 0.1 to 0 and wait without timeout.
# R6. SOURCE_POS_WAIT should accept two, three, or four arguments.

--connection slave

#
# SOURCE_POS_WAIT: Check if time-out is correctly handled.
#
eval SELECT $source_pos_wait_function('master-bin.999999', 0, 1);
eval EXPLAIN select $source_pos_wait_function('master-bin.999999', 0, 1);

#
# SOURCE_POS_WAIT (BUG#651): Check if it hangs when replica is
# idle and STOP REPLICA is issued.
#
send_eval SELECT $source_pos_wait_function('master-bin.999999',0);
connection slave1;
--source include/stop_slave_sql.inc
connection slave;
reap;

#
#  SOURCE_POS_WAIT (BUG#26622): it does not work as documented
#
connection master;
echo "*** must be empty ***";
query_vertical SHOW REPLICA STATUS;

echo "*** must be NULL ***";
eval select $source_pos_wait_function('foo', 98);

#
# Bug#24976304 WAIT_UNTIL_SQL_THREAD_AFTER_GTIDS TIMEOUT VALUE HANDLES SOME
# INPUTS BADLY
#
connection slave;
--source include/start_slave_sql.inc

--echo Test SOURCE_POS_WAIT function with negative timeout value.
--echo When SQL_MODE is strict, then it will throw WRONG_ARGUMENTS
--echo error.
SET @save_sql_mode = @@SESSION.SQL_MODE;
SET @@SESSION.SQL_MODE = "STRICT_ALL_TABLES";
--error ER_WRONG_ARGUMENTS
eval SELECT $source_pos_wait_function('master-bin.999999', 4, -1);

#--echo When SQL_MODE is non-strict mode, then it will return NULL immediately
#--echo without doing any work and generates WRONG_ARGUMENTS warning.
SET @@session.sql_mode = "";
eval SELECT $source_pos_wait_function('master-bin.999999', 4, -1);
SET @@session.sql_mode = @save_sql_mode;

--echo Test WAIT_FOR_EXECUTED_GTID_SET function with fractional timeout value.
--echo Earlier(before fix), 0.4 is read as integer value '0' and the function
--echo hanged forever (or till all the specified gtids are executed).
--echo Now(after fix) 0.4 will be read as doube 0.4 and will waitfor
--echo atleast 0.4 seconds (or till all the specified  gtids are executed).
--echo Also check that in case of timeout, function returns -1.
--let $assert_text = $source_pos_wait_function returns -1 if the timeout has been exceeded.
--let $query_result = query_get_value("SELECT $source_pos_wait_function('master-bin.999999', 4, 0.4) AS VALUE", VALUE, 1)
--let $assert_cond = "$query_result" = "-1"
--source include/assert.inc

# End of Test Bug#24976304

#
# Coverage: Use fourth parameter
#
eval SELECT $source_pos_wait_function('master-bin.999999', 4, 1, "non_existent");
