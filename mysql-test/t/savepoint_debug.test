--source include/have_debug.inc

--echo #
--echo # Bug#27898591: SQL_ERROR.CC:380: VOID DIAGNOSTICS_AREA::SET_OK_STATUS(ULONGLONG, ULONGLONG, CON
--echo #

START TRANSACTION;
SAVEPOINT save_1;
SET DEBUG="+d,fail_ha_release_savepoint";
--error ER_UNKNOWN_ERROR
SAVEPOINT save_1;
SET DEBUG="-d,fail_ha_release_savepoint";
