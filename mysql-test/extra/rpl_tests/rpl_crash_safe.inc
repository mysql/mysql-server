#
# This file relies on the rpl_mixing_engines.inc which provides an engine
# to process commands.
#

#
# Creates tables used throughout the test and changes the type of the
# mysql.slave_relay_log_info to Innodb.
#
if (`SELECT HEX(@commands) = HEX('configure')`)
{
  --sync_slave_with_master
  SHOW CREATE TABLE mysql.slave_relay_log_info;
  ALTER TABLE mysql.slave_relay_log_info ENGINE= Innodb;
  SHOW CREATE TABLE mysql.slave_relay_log_info;

  connection master;
  --source extra/rpl_tests/rpl_mixing_engines.inc

  --sync_slave_with_master
  connection master;
}

#
# Cleans the test case by deleting all tables, triggers, procedures and
# functions that were created.
#
if (`SELECT HEX(@commands) = HEX('clean')`)
{
  connection master;
  --source extra/rpl_tests/rpl_mixing_engines.inc
}

#
# Executes the set of commands defined in @command by calling
# rpl_mixing_engines.inc
#
if (`SELECT HEX(@commands) != HEX('')`)
{
  # 
  # If an fault injection point was defined, stop the SQL THREAD and
  # prepare the slave to be restarted. Otherwise, do nothing.
  #
  if (`SELECT HEX(@failure) != HEX('')`)
  {
    connection slave;
    STOP SLAVE SQL_THREAD;
    source include/wait_for_slave_sql_to_stop.inc;
    --eval SET GLOBAL debug="$failure";
    --exec echo "wait" > $MYSQLTEST_VARDIR/tmp/mysqld.2.expect
  }

  #
  # Prints the expected outcome after the recovery. Check the
  # WL#4801 for further details.
  #
  connection master;
  if (`SELECT HEX(@failure) != HEX('')`)
  {
    if (`SELECT HEX(@failure) != HEX('d,crash_after_commit_and_update_pos') && HEX(@failure) != HEX('d,crash_after_apply')`)
    {
      --echo FAILURE $failure and OUTCOME O1
    }
    if (`SELECT HEX(@failure) = HEX('d,crash_after_commit_and_update_pos') || HEX(@failure) = HEX('d,crash_after_apply')`)
    {
      --echo FAILURE $failure and OUTCOME O2
    }
  }

  #
  # Executes the set of commands defined in @command by calling
  # rpl_mixing_engines.inc
  #
  connection master;
  let $master_before= query_get_value(SHOW MASTER STATUS, Position, 1);
  --source extra/rpl_tests/rpl_mixing_engines.inc
  let $master_after= query_get_value(SHOW MASTER STATUS, Position, 1);

  #
  # Restarts the SQL THREAD and waits for the SLAVE to be crashed and
  # re-started.
  #
  connection master;
  if (`SELECT HEX(@failure) != HEX('')`)
  {
    connection slave;
    --error 0, 2013
    START SLAVE SQL_THREAD;
    --source include/wait_until_disconnected.inc
    --exec echo "restart" > $MYSQLTEST_VARDIR/tmp/mysqld.2.expect
    --let $rpl_server_number= 2
    --source include/rpl_reconnect.inc
    connection slave;
    --let $slave_before= `SELECT master_log_pos FROM mysql.slave_relay_log_info`
    START SLAVE;
    source include/wait_for_slave_to_start.inc;
  }

  #
  # Syncs the slave with the master and checks if the slaves applied all
  # changes from the master and is not out of sync.
  #
  connection master;
  sync_slave_with_master;
  --let $slave_after= `SELECT master_log_pos FROM mysql.slave_relay_log_info`
  connection master;
  if (`SELECT $slave_after != $master_after`)
  {
    --echo Slave is out of sync.
    --echo MASTER_BEFORE $master_before MASTER_AFTER $master_after SLAVE_BEFORE $slave_before SLAVE_AFTER $slave_after
    --source include/show_rpl_debug_info.inc
    --die
  }
}

connection master;
