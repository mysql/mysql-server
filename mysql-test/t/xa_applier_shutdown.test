# This test checks the behavior of applier thread when shutdown is issued at
# various stages of the XA transaction. The session thread is marked as an
# applier thread by setting the pseudo_replica_mode and executing BINLOG''
# query.
#
# References:
#     Bug#32416819: ASSERTION "UT_LIST_GET_LEN.TRX_SYS->MYSQL_TRX_LIST. == 0" ON
#     SHUTDOWN
--source include/big_test.inc

--echo # Scenario 1: Simple test with CREATE TABLE before XA START
--echo # 1.1: Shutdown from same connection
--let $create_table = 1
--let $insert_table = 0
--let $need_xa_end = 0
--let $need_xa_prepare = 0
--let $need_xa_commit = 0
--let $need_xa_rollback = 0
--let $restart_connection = 0
--source include/xa_applier_shutdown.inc

--echo # 1.2: Shutdown from other connection
--let $create_table = 1
--let $insert_table = 0
--let $need_xa_end = 0
--let $need_xa_prepare = 0
--let $need_xa_commit = 0
--let $need_xa_rollback = 0
--let $restart_connection = conn_case_1_2
--source include/xa_applier_shutdown.inc


--echo # Scenario 2: After XA START
--echo # 2.1: Shutdown from same connection
--let $create_table = 0
--let $insert_table = 0
--let $need_xa_end = 0
--let $need_xa_prepare = 0
--let $need_xa_commit = 0
--let $need_xa_rollback = 0
--let $restart_connection = 0
--source include/xa_applier_shutdown.inc

--echo # 2.2: Shutdown from other connection
--let $create_table = 0
--let $insert_table = 0
--let $need_xa_end = 0
--let $need_xa_prepare = 0
--let $need_xa_commit = 0
--let $need_xa_rollback = 0
--let $restart_connection = conn_case_2_2
--source include/xa_applier_shutdown.inc


--echo # Scenario 3: After XA START + XA END
# restart_mysqld.inc checks for pid_file with `SHOW VARIABLES LIKE 'pid_file';`.
# However, since XA transaction is not yet prepared, this statement will fail.
# Hence it is necessary that the restart is issued from another connection.
--let $create_table = 0
--let $insert_table = 0
--let $need_xa_end = 1
--let $need_xa_prepare = 0
--let $need_xa_commit = 0
--let $need_xa_rollback = 0
--let $restart_connection = conn_case_3
--source include/xa_applier_shutdown.inc

--echo # Scenario 4: After XA START + XA END + XA PREPARE
--echo # 4.1: Shutdown from same connection
--let $create_table = 0
--let $insert_table = 0
--let $need_xa_end = 1
--let $need_xa_prepare = 1
--let $need_xa_commit = 0
--let $need_xa_rollback = 0
--let $restart_connection = 0
--source include/xa_applier_shutdown.inc

--echo # 4.2: Shutdown from other connection
--let $create_table = 0
--let $insert_table = 0
--let $need_xa_end = 1
--let $need_xa_prepare = 1
--let $need_xa_commit = 0
--let $need_xa_rollback = 0
--let $restart_connection = conn_case_4_2
--source include/xa_applier_shutdown.inc


--echo # Scenario 5: After XA START + INSERT
--echo # 5.1: Shutdown from same connection
--let $create_table = 1
--let $insert_table = 1
--let $need_xa_end = 0
--let $need_xa_prepare = 0
--let $need_xa_commit = 0
--let $need_xa_rollback = 0
--let $restart_connection = 0
--source include/xa_applier_shutdown.inc

--echo # 5.2: Shutdown from other connection
--let $create_table = 1
--let $insert_table = 1
--let $need_xa_end = 0
--let $need_xa_prepare = 0
--let $need_xa_commit = 0
--let $need_xa_rollback = 0
--let $restart_connection = conn_case_5_2
--source include/xa_applier_shutdown.inc


--echo # Scenario 6: After XA START + INSERT + XA END
# Restart from other connection is mandatory here, similar to case 3
--let $create_table = 1
--let $insert_table = 1
--let $need_xa_end = 1
--let $need_xa_prepare = 0
--let $need_xa_commit = 0
--let $need_xa_rollback = 0
--let $restart_connection = conn_case_6
--source include/xa_applier_shutdown.inc

--echo # Scenario 7: After XA START + INSERT + XA END + XA PREPARE, do restart
--echo #   followed by XA ROLLBACK
--echo # 7.1: Shutdown from same connection
--let $create_table = 1
--let $insert_table = 1
--let $need_xa_end = 1
--let $need_xa_prepare = 1
--let $need_xa_commit = 0
--let $need_xa_rollback = 1
--let $restart_connection = 0
--source include/xa_applier_shutdown.inc

--echo # 7.2: Shutdown from other connection
--let $create_table = 1
--let $insert_table = 1
--let $need_xa_end = 1
--let $need_xa_prepare = 1
--let $need_xa_commit = 0
--let $need_xa_rollback = 1
--let $restart_connection = conn_case_7_2
--source include/xa_applier_shutdown.inc

--echo # Scenario 8: After XA START + INSERT + XA END + XA PREPARE, do restart
--echo #   followed by XA COMMIT
--echo # 8.1: Shutdown from same connection
call mtr.add_suppression("Found 1 prepared XA transactions");
--let $create_table = 1
--let $insert_table = 1
--let $need_xa_end = 1
--let $need_xa_prepare = 1
--let $need_xa_commit = 1
--let $need_xa_rollback = 0
--let $restart_connection = 0
--source include/xa_applier_shutdown.inc

--echo # 8.2: Shutdown from other connection
--let $create_table = 1
--let $insert_table = 1
--let $need_xa_end = 1
--let $need_xa_prepare = 1
--let $need_xa_commit = 1
--let $need_xa_rollback = 0
--let $restart_connection = conn_case_8_2
--source include/xa_applier_shutdown.inc

--echo # Secenario 9: XA START + XA END + XA COMMIT ONE PHASE, do restart from
--echo #   from other connection
--echo # 9.1: Without INSERTs
# restart_mysqld.inc checks for pid_file with `SHOW VARIABLES LIKE 'pid_file';`.
# However, since XA transaction is not yet prepared, this statement will fail.
# Hence it is necessary that the restart is issued from another connection.
--let $create_table = 1
--let $insert_table = 0
--let $need_xa_end = 1
--let $need_xa_commit_one = 1
--let $need_xa_prepare = 0
--let $need_xa_commit = 0
--let $need_xa_rollback = 0
--let $restart_connection = conn_case_9_1
--source include/xa_applier_shutdown.inc

--echo # 9.2: With INSERTs
# restart_mysqld.inc checks for pid_file with `SHOW VARIABLES LIKE 'pid_file';`.
# However, since XA transaction is not yet prepared, this statement will fail.
# Hence it is necessary that the restart is issued from another connection.
--let $create_table = 1
--let $insert_table = 1
--let $need_xa_end = 1
--let $need_xa_commit_one = 1
--let $need_xa_prepare = 0
--let $need_xa_commit = 0
--let $need_xa_rollback = 0
--let $restart_connection = conn_case_9_2
--source include/xa_applier_shutdown.inc
