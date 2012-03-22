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
# See rpl_mixing_engines.inc.
#
# ==== Implementation ====
#
# This file has its own code for 'configure' and 'clean'.  Other
# commands are delegated to rpl_mixing_engines.inc.

--let $rmcs_initial_commands= $commands

#
# Creates tables used throughout the test and changes the type of the
# mysql.slave_relay_log_info to Innodb.
#
if ($commands == configure)
{
  --echo ==== begin rpl_mts_crash_safe.inc:configure ====
  --let $count_database=1
  while ($count_database <= $max_workers)
  {
    --echo ---- begin configure database test_$count_database ----
    --connection master
    --let $commands= configure
    --let $database_name=test_$count_database
    --source extra/rpl_tests/rpl_mixing_engines.inc
    --echo ---- end configure database test_$count_database ----
    inc $count_database;
  }

  --sync_slave_with_master
  --source include/stop_slave.inc
  let $saved_workers= `SELECT @@global.slave_parallel_workers`;
  let $saved_checkpoint= `SELECT @@global.slave_checkpoint_period`;

  SHOW CREATE TABLE mysql.slave_master_info;
  SHOW CREATE TABLE mysql.slave_relay_log_info;
  SHOW CREATE TABLE mysql.slave_worker_info;
  ALTER TABLE mysql.slave_master_info ENGINE= Innodb;
  ALTER TABLE mysql.slave_relay_log_info ENGINE= Innodb;
  ALTER TABLE mysql.slave_worker_info ENGINE= Innodb;
  SHOW CREATE TABLE mysql.slave_master_info;
  SHOW CREATE TABLE mysql.slave_relay_log_info;
  SHOW CREATE TABLE mysql.slave_worker_info;
  --let $commands=
  --echo ==== end rpl_mts_crash_safe.inc:configure ====
}

if ($commands == clean)
{
  --echo ==== begin rpl_mts_crash_safe.inc:clean ====
  --connection slave
  --source include/stop_slave.inc
  --disable_query_log
  eval SET @@global.slave_parallel_workers= $saved_workers;
  eval SET @@global.slave_checkpoint_period= $saved_checkpoint;
  --enable_query_log
  --source include/start_slave.inc

  --connection master
  --let $count_database=1
  while ($count_database <= $max_workers)
  {
    --echo ---- begin clean database test_$count_database ----
    --let $commands= clean
    --let $database_name=test_$count_database
    --source extra/rpl_tests/rpl_mixing_engines.inc
    --echo ---- end clean database test_$count_database ----
    inc $count_database;
  }
  --let $commands=
  --echo ==== end rpl_mts_crash_safe.inc:clean ====
}

if ($commands == recovery)
{
  --echo ==== begin rpl_mts_crash_safe.inc:recovery ====
  --connection master
  let $m_before_master_file= query_get_value("SHOW MASTER STATUS", "File", 1);
  let $m_before_master_pos= query_get_value("SHOW MASTER STATUS", "Position", 1);

  #### Guaranteeing that all events in the binary log are transfered to the slave ######
  --connection slave
  let $saved_current_workers= `SELECT @@global.slave_parallel_workers`;
  let $saved_current_checkpoint= `SELECT @@global.slave_checkpoint_period`;
  SET GLOBAL debug="d,rotate_slave_debug_group";
  --source include/start_slave_io.inc
  let $wait_condition=SELECT Master_log_pos = $m_before_master_pos AND Master_log_name = "$m_before_master_file" FROM mysql.slave_master_info;
  --source include/wait_condition.inc

  #### Crashing server after assigning and processing jobs ######
  --connection slave
  eval SET @@global.slave_checkpoint_group= $count_debug_groups;
  SET GLOBAL debug="d,check_slave_debug_group";
  SET GLOBAL debug="+d,mts_distribute_round_robin";
  --exec echo "wait" > $MYSQLTEST_VARDIR/tmp/mysqld.2.expect
  --error 0, 2013
  START SLAVE SQL_THREAD;
  --source include/wait_until_disconnected.inc
  --exec echo "restart --skip-slave-start --slave-transaction-retries=0 --relay-log-info-repository=TABLE --master-info-repository=TABLE --sync-master-info=1" > $MYSQLTEST_VARDIR/tmp/mysqld.2.expect
  --let $rpl_server_number= 2
  --source include/rpl_reconnect.inc

  #### Checking if the recovery worked and the coordinator has an updated state. ####
  --connection slave
  --source include/start_slave_io.inc
  let $wait_condition=SELECT Master_log_pos = $m_before_master_pos AND Master_log_name = "$m_before_master_file" FROM mysql.slave_master_info;
  --source include/wait_condition.inc
  --enable_query_log

  --source include/start_slave_sql.inc
  let $wait_condition=SELECT Master_log_pos = $m_before_master_pos AND Master_log_name = "$m_before_master_file" FROM mysql.slave_relay_log_info;
  --source include/wait_condition.inc

  --source include/stop_slave.inc
  eval SET @@global.slave_parallel_workers= $saved_current_workers;
  eval SET @@global.slave_checkpoint_period= $saved_current_checkpoint;

  --let $commands=
  --echo ==== end rpl_mts_crash_safe.inc:recovery ====
}

if ($commands != '')
{
  --echo ==== begin rpl_mts_crash_safe.inc:rpl_mixing_engines.inc ====
  --connection master
  --source extra/rpl_tests/rpl_mixing_engines.inc
  --echo ==== end rpl_mts_crash_safe.inc:rpl_mixing_engines.inc ====
}

--let $commands= $rmcs_initial_commands
