#
# BUG#24594140: VIEW GETS DROPPED IF ALTER VIEW FAILS WITH
#               ERRNO 1213 (ER_LOCK_DEADLOCK)
#
CREATE TABLE test.t1(fld1 INT);
CREATE VIEW test.v1 AS SELECT * FROM test.t1;
SET SESSION debug= "+d, inject_error_ha_write_row";
ALTER VIEW test.v1 AS SELECT * FROM test.t1;
ERROR HY000: Got error 122 - 'Internal (unspecified) error in handler' from storage engine
SET SESSION debug= "-d, inject_error_ha_write_row";
# Without patch, results in 'v1' not found error
# since the view is dropped but the re-create failed
# during the above ALTER VIEW.
ALTER VIEW test.v1 AS SELECT * FROM test.t1;
# Cleanup.
DROP VIEW test.v1;
DROP TABLE test.t1;
#
# Bug#27041350: ASSERTION `THD->IS_SYSTEM_THREAD() || THD->KILLED ||
# THD->IS_ERROR()' FAILED
#
# Stack overrun was incorrectly masked when
# View_metadata_updater_error_handler was active.
CREATE VIEW v1 AS SELECT 1 FROM DUAL;
CREATE TABLE t1(c INT,d INT,KEY(c));
ALTER DEFINER=s@1 VIEW v1 AS SELECT * FROM t1;
Warnings:
Note	1449	The user specified as a definer ('s'@'1') does not exist
# Enable stack overrun simulation, after having installed
# View_metadata_updater_error_handler. Should not trigger assert.
SET SESSION DEBUG= "+d,enable_stack_overrun_simulation";
ALTER TABLE t1 KEY_BLOCK_SIZE=8;
Got one of the listed errors
# Cleanup
SET SESSION DEBUG= "-d,enable_stack_overrun_simulation";
DROP TABLE t1;
DROP VIEW v1;
