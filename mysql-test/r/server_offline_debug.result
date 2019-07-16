#
#  BUG#28761869 - LOCK_ORDER: DEADLOCK INVOLVING LOCK_OFFLINE_MODE.
#
SET @global_saved_tmp =  @@global.offline_mode;
SET DEBUG_SYNC='after_lock_offline_mode_acquire SIGNAL lock_offline_mode_acquired WAIT_FOR lock_thd_data_acquired';
SET GLOBAL offline_mode=ON;;
SET DEBUG_SYNC='now WAIT_FOR lock_offline_mode_acquired';
SET DEBUG_SYNC='materialize_session_variable_array_THD_locked SIGNAL lock_thd_data_acquired';
SHOW VARIABLES LIKE 'offline_mode';;
# reaping execution status for SET GLOBAL offline_mode=ON;
Variable_name	Value
offline_mode	ON
SET DEBUG_SYNC='RESET';
# Restoring the original values.
SET @@global.offline_mode = @global_saved_tmp;
#
# Bug #29849046: ASSERT AT SQL_PARSE.CC:6421 IF NON-PRIVILEGED USER
#   TRIES ENABLING OFFLINE_MODE
SET @global_saved_tmp =  @@global.offline_mode;
CREATE USER u1@localhost;
GRANT ALL PRIVILEGES ON *.* to u1@localhost;
REVOKE CONNECTION_ADMIN, SUPER ON *.* FROM u1@localhost;
Warnings:
Warning	1287	The SUPER privilege identifier is deprecated
# in connection con_u1
# test: must not assert
set global offline_mode=on;
# Should have killed itself
SELECT 1 FROM DUAL;
Got one of the listed errors
in connection default
SET @@global.offline_mode = @global_saved_tmp;
DROP USER u1@localhost;
# End of 8.0 tests
