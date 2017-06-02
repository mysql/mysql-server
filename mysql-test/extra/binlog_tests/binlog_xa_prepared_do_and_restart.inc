#
# This file creates various kinds of prepared XA transactions,
# manipulates their connection state and examines how their prepared
# status behave while the transaction is disconnected, killed or
# the server kisses it shutdown.
# The file can be sourced multiple times
# param $restart_number (as the number of inclusion) adjusts
# verification logics.
#
# param [in] $conn_number      Total number of connection each performing
#                              one insert into table.
# param [in] $commit_number    Number of commits from either.
#                              side of the server restart.
# param [in] $rollback_number  The same as the above just for rollback.
# param [in] $term_number      Number of transaction that are terminated
#                              before server restarts
# param [in] $killed_number    Instead of disconnect make some
#                              connections killed when their
#                              transactions got prepared.
# param [in] $server_disconn_number  Make some connections disconnected
#                              by shutdown rather than actively
# param [in] $post_restart_conn_number  Number a "warmup" connection
#                              after server restart, they all commit
# param [out] restart_number   Counter to be incremented at the end of the test
#

# The test consists of three sections:
# I.   Corner cases check
# II.  Regular case check
# III. Post server-restart verification


#
# I. Corner cases of
#
# A. XA with an update to a temp table
# B. XA with SELECT
# C. XA empty
# Demonstrate their XA status upon prepare and how they react on disconnect and
# shutdown.
# In each of A,B,C three prepared transactions are set up.
# trx1 is for disconnection, trx2 for shutdown, trx3 for being killed.
# The A case additionally contains some XA prohibited state transaction check.
#
# D. Prove that not prepared XA remains to be cleared out by disconnection.
#

#
# A. The temp table only prepared XA recovers only formally to
#    let post recovery XA COMMIT or XA ROLLBACK with no effect.

--let $type = tmp
--let $index = 1
--let $sql_init1 = SET @@sql_log_bin = OFF
--let $sql_init2 = CREATE TEMPORARY TABLE tmp$index (a int) ENGINE=innodb
--let $sql_doit  = INSERT INTO tmp$index SET a=$index
--source extra/binlog_tests/binlog_xa_prepare_connection.inc

--let $index = 2
--source extra/binlog_tests/binlog_xa_prepare_connection.inc

--let $index = 3
--source extra/binlog_tests/binlog_xa_prepare_connection.inc
--let $conn3_id=`SELECT connection_id()`

#
# Various prohibited XA state changes to test here:
#

--connection default
# Stealing is not allowed
--error ER_XAER_NOTA
--eval  XA COMMIT 'trx1$type'
--error ER_XAER_NOTA
--eval  XA ROLLBACK 'trx1$type'

# Before disconnect: creating a duplicate is not allowed
--error ER_XAER_DUPID
--eval  XA START 'trx1$type'

# Manipulate now the prepared transactions.
# Two to terminate, one to leave out.
--let $terminate_with = XA COMMIT
--let $num_trx_prepared = $index
--source extra/binlog_tests/binlog_xa_prepare_disconnect.inc

#
# B. "Read-only" (select) prepared XA recovers only formally to
#    let post recovery XA COMMIT or XA ROLLBACK with no effect.
#
--let $type=ro1
--let $index = 1
--let $sql_init1 =
--let $sql_init2 =
--let $sql_doit  = SELECT * from t ORDER BY a
--source extra/binlog_tests/binlog_xa_prepare_connection.inc

--let $index = 2
--source extra/binlog_tests/binlog_xa_prepare_connection.inc

--let $index = 3
--source extra/binlog_tests/binlog_xa_prepare_connection.inc
--let $conn3_id=`SELECT connection_id()`

--let $terminate_with = XA COMMIT
# two three above section prepared transaction were terminated.
--inc $num_trx_prepared
--source extra/binlog_tests/binlog_xa_prepare_disconnect.inc

--let $type=ro2
--let $index = 1
--source extra/binlog_tests/binlog_xa_prepare_connection.inc

--let $index = 2
--source extra/binlog_tests/binlog_xa_prepare_connection.inc

--let $index = 3
--source extra/binlog_tests/binlog_xa_prepare_connection.inc
--let $conn3_id=`SELECT connection_id()`

--let $terminate_with = XA ROLLBACK
# two three above section prepared transaction were terminated.
--inc $num_trx_prepared
--source extra/binlog_tests/binlog_xa_prepare_disconnect.inc

#
# C. Empty prepared XA recovers only formally to
#    let post recovery XA COMMIT or XA ROLLBACK with no effect.
#
--let $type=empty1
--let $index = 1
--let $sql_init1 =
--let $sql_init2 =
--let $sql_doit  =
--source extra/binlog_tests/binlog_xa_prepare_connection.inc

--let $index = 2
--source extra/binlog_tests/binlog_xa_prepare_connection.inc

--let $index = 3
--source extra/binlog_tests/binlog_xa_prepare_connection.inc
--let $conn3_id=`SELECT connection_id()`

--let $terminate_with = XA COMMIT
--inc $num_trx_prepared
--source extra/binlog_tests/binlog_xa_prepare_disconnect.inc

--let $type=empty2
--let $index = 1
--source extra/binlog_tests/binlog_xa_prepare_connection.inc

--let $index = 2
--source extra/binlog_tests/binlog_xa_prepare_connection.inc

--let $index = 3
--source extra/binlog_tests/binlog_xa_prepare_connection.inc
--let $conn3_id=`SELECT connection_id()`

--let $terminate_with = XA ROLLBACK
--inc $num_trx_prepared
--source extra/binlog_tests/binlog_xa_prepare_disconnect.inc

#
# D. Not prepared XA disconnects to be cleared out,
#    no effect on data left as well.
#    Few more prohibited XA state transactions is checked out.
#
--let $type=unprepared
--let $prev_count=`SELECT count(*) from t`

--connect(conn1$type, 127.0.0.1,root,,test,$MASTER_MYPORT,)
--eval XA START   'trx1$type'
INSERT INTO t set a=0;
--eval XA END     'trx1$type'

--error ER_XAER_RMFAIL
INSERT INTO t set a=0;
--error ER_XAER_RMFAIL
--eval XA START 'trx1$type'
--error ER_XAER_RMFAIL
--eval XA START 'trx1$type'

--disconnect conn1$type

--connection default
# No such transactions
--error ER_XAER_NOTA
--eval  XA COMMIT 'trx1$type'
if (`SELECT count(*) > $prev_count from t`)
{
    --echo *** Unexpected commit to the table. ***
    --die
}

#
# II. Regular case.
#
# Prepared transactions get disconnected in three ways:
# actively, being killed and by the server shutdown.
#
--let $i=0
while ($i < $conn_number)
{
    --connect (conn$i, 127.0.0.1,root,,test,$MASTER_MYPORT,)
    --let $conn_id=`SELECT connection_id()`
      --disable_reconnect
      SET @@binlog_format = 'STATEMENT';
      if (`SELECT $i % 2`)
      {
         SET @@binlog_format = 'ROW';
      }
    --eval XA START   'trx_$i'
    --disable_warnings
    --eval INSERT INTO t SET a=$i
    --enable_warnings
    --eval XA END     'trx_$i'
    --eval XA PREPARE 'trx_$i'

    --let $disc_via_kill=`SELECT $conn_number - $i <= $killed_number`
    if (!$disc_via_kill)
    {
      --let $disc_via_shutdown=`SELECT $conn_number - $i <= $killed_number + $server_disconn_number`
      if (!$disc_via_shutdown)
      {
        --disconnect conn$i
      }
    }
    if ($disc_via_kill)
    {
      --connection default
      --replace_result $conn_id CONN_ID
      --eval KILL CONNECTION $conn_id
    }

    if (!$disc_via_shutdown)
    {
      --connection default
      --let $wait_condition= SELECT count(*) = 0 FROM v_processlist WHERE PROCESSLIST_ID = $conn_id
      --source include/wait_condition.inc
    }
    --inc $i
}

# [0, $rollback_number - 1] are rolled back now
--connection default

--let $i=0
while ($i < $rollback_number)
{
    --eval XA ROLLBACK 'trx_$i'

    --inc $i
}

# [$rollback_number, $rollback_number + $commit_number - 1] get committed
while ($i < $term_number)
{
    --eval XA COMMIT 'trx_$i'

    --inc $i
}

--source include/$how_to_restart

#
# III. Post server-restart verification.
# It concludes survived XA:s with a number of commits and rollbacks
# as configured in the 1st part to check expected results in the end.
# Cleanup section consists of explicit disconnect (for killed, or
# not disconnected before shutdown).
#

# New XA can be prepared and committed
--let $k = 0
while ($k < $post_restart_conn_number)
{
    --connect (conn_restart_$k, 127.0.0.1,root,,test,$MASTER_MYPORT,)
    --let  $conn_id=`SELECT connection_id()`
    --eval XA START   'new_trx_$k'
    --eval INSERT INTO t SET a=$k
    --eval XA END     'new_trx_$k'
    --eval XA PREPARE 'new_trx_$k'

    --disconnect conn_restart_$k

    --connection default
    --let $wait_condition= SELECT count(*) = 0 FROM v_processlist WHERE PROCESSLIST_ID = $conn_id
    --source include/wait_condition.inc

    --inc $k
}

--connection default
--let $k = 0
while ($k < $post_restart_conn_number)
{
    --eval XA COMMIT  'new_trx_$k'
    --inc $k
}

#
# Symmetrically to the pre-restart, the resurrected trx:s are committed
# [$term_number, $term_number + $commit_number - 1]
# and the rest is rolled back.
#
--let $i = $term_number

while ($i < `SELECT $term_number + $commit_number`)
{
    # Expected to fail
    --error ER_XAER_DUPID
    --eval XA START 'trx_$i'
    --eval XA COMMIT 'trx_$i'
    --inc $i
}

while ($i < $conn_number)
{
    # Expected to fail
    --error ER_XAER_DUPID
    --eval XA START 'trx_$i'
    --eval XA ROLLBACK 'trx_$i'
    --inc $i
}

#
# Verification of correct results of recovered XA transaction handling:
#
SELECT * FROM t;

--let $type=tmp
--disconnect conn2$type
--disconnect conn3$type
--let $type=ro1
--disconnect conn2$type
--disconnect conn3$type
--let $type=ro2
--disconnect conn2$type
--disconnect conn3$type
--let $type=empty1
--disconnect conn2$type
--disconnect conn3$type
--let $type=empty2
--disconnect conn2$type
--disconnect conn3$type

--let $i= $conn_number
--let $k= 0
--let $expl_disconn_number = `SELECT $killed_number + $server_disconn_number`
while ($k < $expl_disconn_number)
{
  --connection default
    --error ER_XAER_NOTA
    --eval  XA ROLLBACK 'trx_$i'

  --dec $i
  --disconnect conn$i

  --inc $k
}

--inc $restart_number
