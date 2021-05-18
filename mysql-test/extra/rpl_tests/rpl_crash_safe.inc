# ==== Purpose ====
#
# This file is an engine to process commands like
# rpl_mixing_engines.inc.  It has the same set of commands, and in
# addition the extra command 'recovery'.  The 'configure' and 'clean'
# commands are also extended version of the same commands in
# rpl_mixing_engines.inc.
#
# ==== Usage ====
#
# --let $commands= command1 command2 ...
# --let $database_name= name
# --let $failure= d,debug_flag
# [--let $verbose= 1]
# --source include/rpl_mixing_engines.inc
#
#   Parameters:
#
#     $commands, $database_name, $verbose
#       See rpl_mixing_engines.inc.
#
#     $failure
#       This debug symbol will be set. See the implementation for
#       details.
#
# ==== Implementation ====
#
# This file has its own code for 'configure' and 'clean'.  For other
# commands, this file first configures the server according to
# $failure, and then delegates the commands to rpl_mixing_engines.inc.

#
# Creates tables used throughout the test and changes the type of the
# mysql.slave_relay_log_info to Innodb.
#
if ($commands == 'configure')
{
  --sync_slave_with_master
  --source include/stop_slave.inc
  SHOW CREATE TABLE mysql.slave_relay_log_info;
  SHOW CREATE TABLE mysql.slave_worker_info;
  ALTER TABLE mysql.slave_relay_log_info ENGINE= Innodb;
  ALTER TABLE mysql.slave_worker_info ENGINE= Innodb;
  SHOW CREATE TABLE mysql.slave_relay_log_info;
  SHOW CREATE TABLE mysql.slave_worker_info;
  --source include/start_slave.inc

  connection master;
  --source extra/rpl_tests/rpl_mixing_engines.inc

  --sync_slave_with_master
  connection master;
  --let $commands=
}

#
# Cleans the test case by deleting all tables, triggers, procedures and
# functions that were created.
#
if ($commands == 'clean')
{
  connection master;
  --source extra/rpl_tests/rpl_mixing_engines.inc
  --let $commands=
}

#
# Executes the set of commands defined in $command by calling
# rpl_mixing_engines.inc
#
if ($commands != '')
{
  # 
  # If an fault injection point was defined, stop the SQL THREAD and
  # prepare the slave to be restarted. Otherwise, do nothing.
  #
  if ($failure != '')
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
  if ($failure != '')
  {
    --let $outcome= O2
    if ($failure != 'd,crash_after_commit_and_update_pos')
    {
      if ($failure != 'd,crash_after_apply')
      {
        if ($failure != 'd,crash_after_commit_before_update_pos')
        {
          --let $outcome= O1
        }
      }
    }
    --echo FAILURE $failure and OUTCOME $outcome
  }

  #
  # Executes the set of commands defined in $command by calling
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
  if ($failure != '')
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
  if ($slave_after != $master_after)
  {
    --echo Slave is out of sync.
    --echo MASTER_BEFORE $master_before MASTER_AFTER $master_after SLAVE_BEFORE $slave_before SLAVE_AFTER $slave_after
    connection slave;
    --source include/show_rpl_debug_info.inc
    --die
  }
}

connection master;
