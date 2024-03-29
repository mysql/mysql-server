--source include/have_debug.inc

--echo #
--echo # BUG#24594140: VIEW GETS DROPPED IF ALTER VIEW FAILS WITH
--echo #               ERRNO 1213 (ER_LOCK_DEADLOCK)
--echo #

CREATE TABLE test.t1(fld1 INT);
CREATE VIEW test.v1 AS SELECT * FROM test.t1;

SET SESSION debug= "+d, inject_error_ha_write_row";
--error ER_GET_ERRNO
ALTER VIEW test.v1 AS SELECT * FROM test.t1;
SET SESSION debug= "-d, inject_error_ha_write_row";

--echo # Without patch, results in 'v1' not found error
--echo # since the view is dropped but the re-create failed
--echo # during the above ALTER VIEW.
ALTER VIEW test.v1 AS SELECT * FROM test.t1;

--echo # Cleanup.
DROP VIEW test.v1;
DROP TABLE test.t1;

--echo #
--echo # Bug#27041350: ASSERTION `THD->IS_SYSTEM_THREAD() || THD->KILLED ||
--echo # THD->IS_ERROR()' FAILED
--echo #
--echo # Stack overrun was incorrectly masked when
--echo # View_metadata_updater_error_handler was active.

CREATE VIEW v1 AS SELECT 1 FROM DUAL;
CREATE TABLE t1(c INT,d INT,KEY(c));
ALTER DEFINER=s@1 VIEW v1 AS SELECT * FROM t1;
--echo # Enable stack overrun simulation, after having installed
--echo # View_metadata_updater_error_handler. Should not trigger assert.
SET SESSION DEBUG= "+d,enable_stack_overrun_simulation";
# Suppress exact error message as it contains numbers which may not be identical
# across platforms
--error ER_STACK_OVERRUN_NEED_MORE,ER_STACK_OVERRUN_NEED_MORE
ALTER TABLE t1 KEY_BLOCK_SIZE=8;

--echo # Cleanup
SET SESSION DEBUG= "-d,enable_stack_overrun_simulation";
DROP TABLE t1;
DROP VIEW v1;

