# This test checks the behavior of applier thread when shutdown is issued at
# various stages of the XA transaction. The session thread is marked as an
# applier thread by setting the pseudo_replica_mode and executing BINLOG''
# query.
#
# References:
#     Bug#32416819: ASSERTION "UT_LIST_GET_LEN.TRX_SYS->MYSQL_TRX_LIST. == 0" ON
#     SHUTDOWN
#
# The include file lists the scenarios to be checked before running shutdown.
# Enable the following flags to check different scenarios:
# $create_table:       Create the table for the test and drop it at the end
# $insert_table:       Insert 3 rows into the table containing single int col
# $need_xa_end:        Includes the XA END statement
# $need_xa_commit_one: Includes the XA COMMIT..ONE PHASE statement followed by
#                      the previous set of statements
# $need_xa_prepare:    Includes the XA PREPARE statement
# $need_xa_commit:     Includes the XA COMMIT statement
# $need_xa_rollback:   Includes the XA ROLLBACK statement
# $restart_connection: If set, create new connection and restart the server
#                      using the new connection
#                      If unset, restart the server on the connection that
#                      executes the transaction
#
# Note:
# During shutdown, Innodb closes trx_sys. At this stage, it is expected that all
# the trx_t's created for mysql is freed (mysql_trx_list - list of trx_t created
# for mysql is empty). Whenever a `USE <database>` command is issued, Innodb
# will create a trx_t (associated with the corresponding session THD) and adds
# it to mysql_trx_list.
#
# During replication, if the replication applier sees an 'XA START', it will
# call detach_native_trx on all handlertons. This function calls
# replace_native_transaction_in_thd by passing nullptr as the new value and
# ha_ptr_backup as the buffer to store the handlerton's THD context. In case of
# InnoDB, the THD context is the trx_t mentioned above.
#
# This context is restored at 3 different places depending on the XA transaction
# 1. In 'XA PREPARE' using attach_native_trx
# 2. In 'XA COMMIT .. ONE PHASE' using ha_commit_low
# 3. In 'XA ROLLBACK' or shutdown using ha_rollback_low
#
# In the bug scenario, the restore was done only when the ha_list was non-empty.
# Hence, if shutdown is issued in middle of processing an empty XA transaction
# (right before XA PREPARE / XA COMMIT .. ONE PHASE), then the reattach step is
# skipped. Thus the trx_t is never returned, leaving the mysql_trx_list
# non-empty when closing trx_sys leading to the assert.
#
# In case of empty transaction, 'XA COMMIT .. ONE PHASE' does not call reattach.
# Hence an 'XA START' after this will attempt to detach again, leading to an
# assertion in the detach function.
#
# The current connection can simulate a replication applier by issuing:
#     SET @@SESSION.pseudo_replica_mode=1;
#     BINLOG '0';
#
# The BINLOG statement will initialize thd->rli_fake, even though the statement
# fails. The thd->rli_fake will simulate the replication applier. '0' is used
# for better readability.

if ($create_table) {
  CREATE TABLE t1 (c1 INT);
  INSERT INTO t1 VALUES (10);
}

SET @@SESSION.pseudo_replica_mode=1;
--error ER_BASE64_DECODE_ERROR
BINLOG '0';

XA START 'test0';

if ($insert_table) {
  INSERT INTO t1 VALUES (1);
  INSERT INTO t1 VALUES (2);
  INSERT INTO t1 VALUES (3);
}

if ($need_xa_end) {
  XA END 'test0';
}

if ($need_xa_commit_one) {
  XA COMMIT 'test0' ONE PHASE;

  XA START 'test0';
  if ($insert_table) {
    INSERT INTO t1 VALUES (4);
    INSERT INTO t1 VALUES (5);
    INSERT INTO t1 VALUES (6);
  }

  if ($need_xa_end) {
    XA END 'test0';
  }
}

if ($need_xa_prepare) {
  XA PREPARE 'test0';
}

XA RECOVER;

if ($restart_connection) {
  # When restarting from a different connection, default connection must be
  # handled separately. In this case, the default connection simply waits until
  # it can reconnect after the server starts.

  --connect ($restart_connection, localhost, root,,)
  --connection $restart_connection
  --source include/restart_mysqld.inc
  --disconnect $restart_connection

  --connection default
  --source include/wait_until_connected_again.inc
}

if (!$restart_connection) {
  # The `connection default` is used to indicate that restart is issued in the
  # current connection

  --connection default
  --source include/restart_mysqld.inc
  --connection default
}

XA RECOVER;

if ($need_xa_rollback) {
  XA ROLLBACK 'test0';
}

if ($need_xa_commit) {
  XA COMMIT 'test0';
}

if ($create_table) {
  SELECT * FROM t1 ORDER BY c1;
  DROP TABLE t1;
}
