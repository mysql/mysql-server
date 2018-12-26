#
# This file disconnects two connections. One actively and one through
# kill. It is included by binlog_xa_prepared_do_and_restart.
#
# param $type              type of transaction
# param $terminate_with    how to conclude actively disconnecte:
#                          XA COMMIT or XA ROLLBACK
# param $conn3_id          connection id of the being killed.
# param $num_trx_prepared  number of transactions prepared so far
#
--connection default

--echo *** $num_trx_prepared prepared transactions must be in the list ***
--replace_column 2 LEN1 3 LEN2 4 TRX_N
XA RECOVER;

--connection conn1$type
--let $conn1_id=`SELECT connection_id()`
--disconnect conn1$type

--connection default
--let $wait_condition= SELECT count(*) = 0 FROM v_processlist WHERE PROCESSLIST_ID = $conn1_id
--source include/wait_condition.inc

# It will conclude now
--eval  $terminate_with 'trx1$type'

--replace_result $conn3_id CONN_ID
--eval KILL connection $conn3_id

--let $wait_condition= SELECT count(*) = 0 FROM v_processlist WHERE PROCESSLIST_ID = $conn3_id
--source include/wait_condition.inc

# It will conclude now
--eval  $terminate_with 'trx3$type'
