# ==== Purpose ====
#
# This file is an engine to process commands like
# common/rpl/mixing_engines.inc.  It has the same set of commands, and in
# addition the extra command 'recovery'.  The 'configure' and 'clean'
# commands are also extended version of the same commands in
# common/rpl/mixing_engines.inc.
#
# ==== Usage ====
#
# See common/rpl/mixing_engines.inc.
#
# ==== Implementation ====
#
# This file has its own code for 'configure' and 'clean'.  Other
# commands are delegated to common/rpl/mixing_engines.inc.

--let $rmcs_initial_commands= $commands

#
# Creates tables used throughout the test and changes the type of the
# mysql.slave_relay_log_info to Innodb.
#
if ($commands == configure)
{
  --echo ==== begin rpl_mta_crash_safe.inc:configure ====
  --let $count_database= 1
  while ($count_database <= $max_workers)
  {
    --echo ---- begin configure database test_$count_database ----
    --connection master
    --let $commands= configure
    --let $database_name= test_$count_database
    --source common/rpl/mixing_engines.inc
    --echo ---- end configure database test_$count_database ----
    inc $count_database;
  }

  --sync_slave_with_master
  --source include/rpl/stop_replica.inc
  let $saved_workers= `SELECT @@global.replica_parallel_workers`;
  let $saved_checkpoint= `SELECT @@global.replica_checkpoint_period`;

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
  --echo ==== end rpl_mta_crash_safe.inc:configure ====
}

if ($commands == clean)
{
  --echo ==== begin rpl_mta_crash_safe.inc:clean ====
  --connection slave
  --source include/rpl/stop_replica.inc
  --disable_query_log
  eval SET @@global.replica_parallel_workers= $saved_workers;
  eval SET @@global.replica_checkpoint_period= $saved_checkpoint;
  --enable_query_log
  --source include/rpl/start_replica.inc

  --connection master
  --let $count_database= 1
  while ($count_database <= $max_workers)
  {
    --echo ---- begin clean database test_$count_database ----
    --let $commands= clean
    --let $database_name= test_$count_database
    --source common/rpl/mixing_engines.inc
    --echo ---- end clean database test_$count_database ----
    inc $count_database;
  }
  --let $commands=
  --echo ==== end rpl_mta_crash_safe.inc:clean ====
}

if ($commands == recovery)
{
  --echo ==== begin rpl_mta_crash_safe.inc:recovery ====
  --connection master
  let $m_before_master_file= query_get_value("SHOW BINARY LOG STATUS", "File", 1);
  let $m_before_master_pos= query_get_value("SHOW BINARY LOG STATUS", "Position", 1);

  #### Guaranteeing that all events in the binary log are transfered to the slave ######
  --connection slave
  let $saved_current_workers= `SELECT @@global.replica_parallel_workers`;
  let $saved_current_checkpoint= `SELECT @@global.replica_checkpoint_period`;
  SET GLOBAL debug="d,rotate_replica_debug_group";
  --source include/rpl/start_receiver.inc
  let $wait_condition=SELECT Master_log_pos = $m_before_master_pos AND Master_log_name = "$m_before_master_file" FROM mysql.slave_master_info;
  let $show_rpl_debug_info= 1;
  --source include/wait_condition_or_abort.inc

  #### Crashing server after assigning and processing jobs ######
  --connection slave

  --let $mts_run=`select @@global.replica_parallel_workers > 0`
  eval SET @@global.replica_checkpoint_group= $count_debug_groups;
  SET GLOBAL debug="d,check_replica_debug_group";
  SET GLOBAL debug="+d,mta_distribute_round_robin";
  --exec echo "wait" > $MYSQLTEST_VARDIR/tmp/mysqld.2.expect
  --error 0, 2013
  START REPLICA SQL_THREAD;
  --source include/wait_until_disconnected.inc

  #### MTS can not relay-log-recover in presense of gaps that the test creates
  #### The server has to be restarted with  --relay-log-recovery = 0 followed by
  #### START REPLICA UNTIL SQL_AFTER_MTS_GAPS.

  if ($mts_run)
  {
    --echo ** fixing gaps **
    --let $rpl_server_number= 2
    --let $rpl_server_parameters= --skip-replica-start --relay-log-recovery=0 --replica-parallel-workers=0

    --source include/rpl/start_server.inc
    # Disable warnings as with GTIDS the clause SQL_AFTER_MTS_GAPS has no effect
    --disable_warnings
    START REPLICA UNTIL SQL_AFTER_MTS_GAPS;
    --enable_warnings
    --let $slave_param= Until_Condition
    --let $slave_param_value= DONE
    --source include/rpl/wait_for_replica_status.inc
    # there's a chance it will hang at this point (thanks to pre-exec UNTIL check)
    --source include/rpl/wait_for_applier_to_stop.inc
  }

  --echo ** regular restart **
  --let $rpl_server_number= 2
  --let $rpl_server_parameters= --skip-replica-start --replica-transaction-retries=0 --sync-source-info=1
  --source include/rpl/restart_server.inc

  #### Checking if the recovery worked and the coordinator has an updated state. ####
  --connection slave
  --source include/rpl/start_receiver.inc
  let $wait_condition=SELECT Master_log_pos = $m_before_master_pos AND Master_log_name = "$m_before_master_file" FROM mysql.slave_master_info;
  --source include/wait_condition_or_abort.inc
  --enable_query_log

  --source include/rpl/start_applier.inc
  let $wait_condition=SELECT Master_log_pos = $m_before_master_pos AND Master_log_name = "$m_before_master_file" FROM mysql.slave_relay_log_info;
  --source include/wait_condition_or_abort.inc

  --source include/rpl/stop_replica.inc
  eval SET @@global.replica_parallel_workers= $saved_current_workers;
  eval SET @@global.replica_checkpoint_period= $saved_current_checkpoint;

  --let $commands=
  --echo ==== end rpl_mta_crash_safe.inc:recovery ====
}

if ($commands != '')
{
  --echo ==== begin rpl_mta_crash_safe.inc:common/rpl/mixing_engines.inc ====
  --connection master
  --source common/rpl/mixing_engines.inc
  --echo ==== end rpl_mta_crash_safe.inc:common/rpl/mixing_engines.inc ====
}

--let $commands= $rmcs_initial_commands
