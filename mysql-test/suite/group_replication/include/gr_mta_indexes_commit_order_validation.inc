# ==== Purpose ====
#
# This include is meant to be used with
# group_replication_parallel_applier_indexes_commit_order test,
# please read it for details.
#
#
# ==== Usage ====
#
# --let $server_number= #
# --source ../include/gr_mta_indexes_commit_order_validation.inc
#
# Parameters:
#   $server_number
#     Server number to connect to.

--echo
--echo # include/gr_mta_indexes_commit_order_validation.inc
--echo # Parameters: server_number: $server_number

if ($server_number == '')
{
  --die ERROR IN TEST: the "server_number" variable must be set
}

if ($logical_timestamps == '')
{
--die ERROR IN TEST: the "logical_timestamps" variable must be set
}

--let $rpl_connection_name= server$server_number
--source include/connection.inc

--echo
--echo # Assert parallel indexes, the 4 groups of transactions
--echo # must be parallel.
--let $binlog_file= server-relay-log-group_replication_applier.000002
--source include/rpl/assert_logical_timestamps.inc

# Stop Group Replication since we will do local operations.
--source include/stop_group_replication.inc

--let $binlog_file= query_get_value(SHOW BINARY LOG STATUS, File, 1)
--let $DATADIR= `SELECT @@DATADIR`
--copy_file $DATADIR/$binlog_file $MYSQL_TMP_DIR/$binlog_file

--echo
--echo # Binary log will be reapplied one by one later through mysql. The
--echo # table is recreated with an extra auto increment field. The new
--echo # field 'id' will be filled by server automatically when reapplying
--echo # binary log. So it exactly records the binlogging order of the
--echo # transactions.
--echo #
DROP TABLE t1;
CREATE TABLE t1 (a INT, id INT AUTO_INCREMENT KEY) ENGINE = InnoDB;

--echo
--echo # Clear GTID_EXECUTED to avoid that transactions reapplied
--echo # from binary log are skipped.
--echo #
RESET BINARY LOGS AND GTIDS;

--echo
--echo # Replay binary log.
--echo #
--let $mysql_server= $MYSQL --defaults-group-suffix=.$server_number
--exec $MYSQL_BINLOG $MYSQL_TMP_DIR/$binlog_file | $mysql_server

--echo
--echo # The transactions should be binlogged in the same order as
--echo # they are binlogged on server 1.
--echo # Because the transactions:
--echo #   * insert numbers starting from 1;
--echo #   * id starts from 1;
--echo #   * both are increased by 1 for each transaction;
--echo # the table's data should look like:
--echo #   id    a
--echo #   1     1
--echo #   2     2
--echo #   ...  ...
--echo #   n     n
--echo # It means the values of field 'id' are always same to the
--echo # values of field 'a'.
SELECT * FROM t1 WHERE id <> a;

--let $assert_text= 'There are no mismatched rows'
--let $assert_cond= [SELECT  COUNT(*) AS count FROM t1 WHERE t1.id <> t1.a, count, 1] = 0
--source include/assert.inc

--let $assert_text= 'There are no row which is null'
--let $assert_cond= [SELECT  COUNT(*) AS count FROM t1 WHERE t1.a is NULL, count, 1] <= 0
--source include/assert.inc

--let $assert_text= 'There are 16 rows'
--let $assert_cond= [SELECT COUNT(*) AS count FROM t1, count, 1] = 16
--source include/assert.inc

DROP TABLE t1;
--remove_file $MYSQL_TMP_DIR/$binlog_file
