--source include/not_threadpool.inc
--source include/have_debug.inc

--echo #
--echo #  BUG#28761869 - LOCK_ORDER: DEADLOCK INVOLVING LOCK_OFFLINE_MODE.
--echo #
# Save the global value to be used to restore the original value.
SET @global_saved_tmp =  @@global.offline_mode;

connect(con1, localhost, root,,);
SET DEBUG_SYNC='after_lock_offline_mode_acquire SIGNAL lock_offline_mode_acquired WAIT_FOR lock_thd_data_acquired';
--send SET GLOBAL offline_mode=ON;
--connection default

SET DEBUG_SYNC='now WAIT_FOR lock_offline_mode_acquired';
SET DEBUG_SYNC='materialize_session_variable_array_THD_locked SIGNAL lock_thd_data_acquired';
--send SHOW VARIABLES LIKE 'offline_mode';

--connection con1
--echo # reaping execution status for SET GLOBAL offline_mode=ON;
--reap;
--connection default
--reap;
--disconnect con1

SET DEBUG_SYNC='RESET';
--echo # Restoring the original values.
SET @@global.offline_mode = @global_saved_tmp;

--echo # End of 8.0 tests
