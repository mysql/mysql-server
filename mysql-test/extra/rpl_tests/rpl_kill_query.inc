# ==== Purpose ====
#
# This is an auxiliary file used by rpl_kill_query.test.
#
# The purpose is to generate a statement that updates a
# non-transactional table, and kill the statement in the middle,
# so that it gets half-executed.
#
# ==== Usage ====
#
# --let $database= DATABASE
# --let $table= TABLE
# --source include/rpl_kill_query.inc
#
# Parameters:
#   $database
#     The current database will be set to $database before executing the
#     killed statement.
#
#   $table
#     The statement will modify this table.
#
# Assumptions:
# - $database should *not* be created before executing this script.
#
# ==== Implementation ====
#
# Apparently, not all statements can be killed in the middle; a simple
# INSERT does not seem to work.  One case that can be killed is an
# INSERT with a trigger; then there is a check for being killed in the
# trigger invocation.
#
# So our approach is to put a BEFORE INSERT trigger on the table.  The
# trigger will execute fast for the first row, and go to sleep for the
# second row. This allows us to kill the query in the middle of the
# sleep. Then, we make sure to wait until *after* the first row is
# inserted, and *before* the sleep is finished.  That ensures that at
# least one row is inserted in the trigger table, and not both rows
# are inserted in the main table, so the statement is half-executed,
# as required.
#
# To sleep on the second row, we use a SLEEP() inside a condition that
# is triggered only for the second row.
#
# To indicate to other threads that the first row has been inserted,
# we do SET @@GLOBAL.BINLOG_FORMAT = ROW before going to sleep.  (It
# does not seem to be enough to wait for rows to be inserted in the
# main table or trigger table, probably since locks prevent other
# threads from seeing the partial insert even in a non-transactional
# table.)  This will not affect execution since the global values is
# only used as default for new connections.
#
# As a sanity-check, we verify that after the query is killed, at most
# one row has been inserted in the main table, and at least one row
# has been inserted in the second table.


--let $_rkq_current_database_old= `SELECT DATABASE()`

--let $assert_cond= @@GLOBAL.BINLOG_FORMAT = "STATEMENT"
--let $assert_text= Test only works if binlog_format is statement
--source include/assert.inc

# Get connection id
--source include/rpl_connection_master.inc
--let $connection_id= `SELECT CONNECTION_ID()`

# Create database, table, and trigger table
eval CREATE DATABASE $database;
eval USE $database;
eval CREATE TABLE $table (a INT) ENGINE = MyISAM;
eval CREATE TABLE trigger_table (a INT) ENGINE = MyISAM;

# This master-only trigger will sleep for N seconds when the value N
# is inserted to a row.
SET SQL_LOG_BIN=0;
--delimiter |
eval
CREATE TRIGGER trig1 BEFORE INSERT ON $table FOR EACH ROW
BEGIN
  IF NEW.a != 0 THEN
    SET @@GLOBAL.BINLOG_FORMAT = 'row';
    DO SLEEP(1000000);
  END IF;
  INSERT INTO trigger_table VALUES (1);
END|
--delimiter ;
SET SQL_LOG_BIN=1;

# Sleep 1000000 seconds before inserting the second row.
--send
eval INSERT INTO $table VALUES (0), (1);

# Wait until the sleep for the second row happens.
--source include/rpl_connection_master1.inc

--let $wait_condition= SELECT @@GLOBAL.BINLOG_FORMAT = 'ROW'
--let $show_rpl_debug_info= 1
--source include/wait_condition.inc

# Kill query.
--replace_result $connection_id <CONNECTION_ID>
eval KILL QUERY $connection_id;

# Get the error message.
--source include/rpl_connection_master.inc
--error ER_QUERY_INTERRUPTED
reap;

SET @@GLOBAL.BINLOG_FORMAT = 'STATEMENT';

# Check that at most one row was inserted in main table.
--let $assert_text= At most one row should be inserted in $database.$table
--let $assert_cond= COUNT(*) <= 1 FROM $database.$table
--source include/assert.inc

# Check that at least one row was inserted in trigger table.
--let $assert_text= At least one row should be inserted in $database.trigger_table
--let $assert_cond= COUNT(*) >= 1 FROM $database.trigger_table
--source include/assert.inc

# Clean up.
SET SQL_LOG_BIN=0;
eval DROP TRIGGER trig1;
SET SQL_LOG_BIN=1;
DROP TABLE trigger_table;
eval DROP TABLE $table;
eval DROP DATABASE $database;
--eval USE $_rkq_current_database_old
