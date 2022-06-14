# ==== Purpose ====
#
# Test case for checking the behavior of binary log cache when encrypted.
#
# It will generate a workload doing many SAVEPOINT/ROLLBACK TO SAVEPOINT
# and then committing the transactions. This will be done with binlog_encryption
# OFF and ON.
#
# After the workloads, it would ensure SHOW BINLOG EVENTS works fine for both
# binary log files and will sync master and slave comparing their tables
# contents.
#
# ==== Related Bugs and Worklogs ====
#
# WL#12079: Binary log cache encryption at rest
#

--disable_query_log
call mtr.add_suppression("Could not find the data corresponding to Data ID:");
--enable_query_log

# Setup test case variables
--let $keyring_master= $MYSQL_TMP_DIR/keyring_master
--let $keyring_slave= $MYSQL_TMP_DIR/keyring_slave
--let $MASTER_DATADIR= `select @@datadir`
--source include/rpl_connection_slave.inc
--let $SLAVE_DATADIR= `select @@datadir`
--source include/rpl_connection_master.inc


--write_file $MYSQLTEST_VARDIR/tmp/rpl_binlog_cache_encryption.inc PROCEDURE
--disable_query_log
--let $trx=50
--echo # Inserting 50 transactions with save points
while ($trx)
{
  BEGIN;
  --let $string=`SELECT REPEAT('MySQL123',1024)`
  --let $savepoints=5
  --let $data_size=400
  while ($savepoints)
  {
    --dec $savepoints
    --let $before_savepoint=20
    while ($before_savepoint > 9)
    {
      --dec $before_savepoint
      --expr $length = $before_savepoint * $data_size
      --eval INSERT INTO t1 (c2) VALUES (LEFT("$string",$length))
    }
    --eval SAVEPOINT point$savepoints
    --let $until_rollback=20
    while ($until_rollback > 9)
    {
      --dec $until_rollback
      --expr $length = $until_rollback * $data_size
      --eval INSERT INTO t1 (c2) VALUES (LEFT("$string",$length))
    }
    --eval ROLLBACK TO SAVEPOINT point$savepoints
    --let $after_rollback=20
    while ($after_rollback > 9)
    {
      --dec $after_rollback
      --expr $length = $after_rollback * $data_size
      --eval INSERT INTO t1 (c2) VALUES (LEFT("$string",$length))
    }
  }
  COMMIT;
  --dec $trx
}
--enable_query_log
#END OF
PROCEDURE

--echo # Part 1 - binlog_encryption = OFF
--let $plain_master_file=query_get_value(SHOW MASTER STATUS, File, 1)
CREATE TABLE t1 (c1 INT PRIMARY KEY AUTO_INCREMENT, c2 TEXT);

--source $MYSQLTEST_VARDIR/tmp/rpl_binlog_cache_encryption.inc

--echo SHOW BINLOG EVENTS shall not fail on the un-encrypted binlog file
--disable_result_log
--replace_result $plain_master_file PLAIN_MASTER_FILE
--eval SHOW BINLOG EVENTS IN '$plain_master_file'
--enable_result_log

--echo # Restarting the server with "--binlog_encryption=ON" and keyring
--let $rpl_server_number= 1
--let $rpl_server_parameters= $RPL_PLUGIN_DIR_OPT --binlog_encryption=ON
--let $rpl_omit_print_server_parameters= 1
--source include/rpl_restart_server.inc

--let $assert_text=binlog_encryption option shall be ON
--let $option_value = `SELECT variable_value FROM performance_schema.global_variables WHERE variable_name = "binlog_encryption"`
--let $assert_cond= "$option_value" = "ON"
--source include/assert.inc

--let $encrypted_master_file=query_get_value(SHOW MASTER STATUS, File, 1)
--let $rpl_log_file=$MASTER_DATADIR$encrypted_master_file
--source include/rpl_get_log_encryption_key_id.inc
--let $assert_text=Binary log is encrypted using 1st master key
--let $assert_cond= RIGHT("$rpl_encryption_key_id", 2) = "_1"
--source include/assert.inc

--echo # Part 2 - binlog_encryption = ON

--source $MYSQLTEST_VARDIR/tmp/rpl_binlog_cache_encryption.inc

--echo SHOW BINLOG EVENTS shall not fail on the encrypted binlog file
--disable_result_log
--replace_result $encrypted_master_file ENCRYPTED_MASTER_FILE
--eval SHOW BINLOG EVENTS IN '$encrypted_master_file'
--enable_result_log

# Restart the server loading the keyring
--echo # Restarting the server without "--binlog_encryption=ON"
--let $rpl_server_number= 1
--let $rpl_server_parameters= $RPL_PLUGIN_DIR_OPT
--let $rpl_omit_print_server_parameters= 1
--source include/rpl_restart_server.inc

--source include/rpl_connection_slave.inc
--source include/start_slave.inc
--source include/rpl_connection_master.inc
--source include/sync_slave_sql_with_master.inc

# Table diff
--let $diff_tables= master:t1, slave:t1
--source include/diff_tables.inc

# Cleanup
--source include/rpl_connection_master.inc
--remove_file $MYSQLTEST_VARDIR/tmp/rpl_binlog_cache_encryption.inc

DROP TABLE t1;
