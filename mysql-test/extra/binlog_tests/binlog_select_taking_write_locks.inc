# ==== Purpose ====
#
# The purpose of this script is to test the behaviour of `SELECT` based
# statements that acquire table write-locks on regards to `GTID_NEXT` and
# `GTID_EXECUTED` state.
#
# ==== Requirements ====
#
# R1. When `GTID_NEXT` = `UNDEFINED`, executing a select statement which
#     acquires write-locks will make the stament fail as a non-innocent
#     statement, that is, it is required for the `GTID_NEXT` to be set to
#     `AUTOMATIC` or `UUID:number`.
#
# R2. Despite the behaviour imposed by *R1*, executing a select statement that
#     acquires write-locks will not trigger a change in `GTID_NEXT`,
#     `GTID_OWNED` or `GTID_EXECUTED`.
#
# ==== Implementation ====
#
# TC.1 `GTID_NEXT` = `UNDEFINED` + Query statement acquiring wr-lock
# ------------------------------------------------------------------
# 1) SET `GTID_NEXT` to a manual value.
# 2) Execute an implicitly commiting statement.
# 3) Execute `SELECT ... FOR UPDATE`, expecting an error -
#    `ER_GTID_NEXT_TYPE_UNDEFINED_GTID`
#
# TC.2 `GTID_NEXT` = `ASSIGNED` + Query statement acquiring wr-lock
# -----------------------------------------------------------------
# 1) SET `GTID_NEXT` to a manual value.
# 2) Execute `SELECT ... FOR UPDATE`.
# 3) Assert that `GTID_NEXT`, `GTID_OWNED` and `GTID_EXECUTED` remain unaltered.
# 4) Execute `SET GTID_NEXT = "AUTOMATIC"` expecting an error -
#    `ER_CANT_SET_GTID_NEXT_WHEN_OWNING_GTID`
# 5) Commit in order to release `GTID_NEXT`.
#
# TC.3 `GTID_NEXT` = `AUTOMATIC` + Query statement acquiring wr-lock
# ------------------------------------------------------------------
# 1) SET `GTID_NEXT` to "AUTOMATIC".
# 2) Execute an implicitly commiting statement.
# 3) Execute `SELECT ... FOR UPDATE`.
# 4) Assert that `GITD_OWNED` and `GTID_EXECUTED` remain unaltered.
#
# ==== References ====
#
# binlog_gtid.binlog_gtid_select_taking_write_locks
#
# binlog_nogtid.binlog_nogtid_select_taking_write_locks
#
# BUG#27903848 [MYSQL 8.0 GA DEBUG BUILD] ASSERTION `GTID_NEXT_TYPE ==
# ANONYMOUS_GTID' FAILED.
#

CREATE TABLE t1(c1 INT) ENGINE=InnoDB;
CREATE TABLE t2(c1 INT) ENGINE=MEMORY;

--echo # TC.1 `GTID_NEXT` = `UNDEFINED` + Query statement acquiring wr-lock
--echo # ------------------------------------------------------------------

# 1) SET `GTID_NEXT` to a manual value.
SET GTID_NEXT='aaaaaaaa-aaaa-aaaa-aaaa-bbbbbbbbbbbb:1';

# 2) Execute an implicitly commiting statement.
CREATE TABLE t3(c1 CHAR CHARACTER SET utf16) ENGINE=InnoDB;

# 3) Execute `SELECT ... FOR UPDATE`, expecting an error -
#    `ER_GTID_NEXT_TYPE_UNDEFINED_GTID`
--error ER_GTID_NEXT_TYPE_UNDEFINED_GTID
SELECT * FROM t1, t2, t3 FOR UPDATE OF t1, t2, t3 SKIP LOCKED;

--echo # TC.2 `GTID_NEXT` = `ASSIGNED` + Query statement acquiring wr-lock
--echo # -----------------------------------------------------------------

# 1) SET `GTID_NEXT` to a manual value.
SET GTID_NEXT='aaaaaaaa-aaaa-aaaa-aaaa-bbbbbbbbbbbb:2';

# 2) Execute `SELECT ... FOR UPDATE`.
--let $gtid_executed_before = `SELECT @@GTID_EXECUTED`
--let $gtid_owned_before = `SELECT @@GTID_OWNED`
SELECT * FROM t1, t3 FOR UPDATE OF t1, t3 SKIP LOCKED;

# 3) Assert that `GTID_NEXT`, `GTID_OWNED` and `GTID_EXECUTED` remain unaltered.
--let $assert_text = No change in `GTID_EXECUTED`
--let $assert_cond = "[SELECT @@GTID_EXECUTED]" = "$gtid_executed_before"
--source include/assert.inc
--let $assert_text = No change in `GTID_OWNED`
--let $assert_cond = "[SELECT @@GTID_OWNED]" = "$gtid_owned_before"
--source include/assert.inc
--let $assert_text = No change in `GTID_NEXT`
--let $assert_cond = "[SELECT @@GTID_NEXT]" = "aaaaaaaa-aaaa-aaaa-aaaa-bbbbbbbbbbbb:2"
--source include/assert.inc

# 4) Execute `SET GTID_NEXT = "AUTOMATIC"` expecting an error -
#    `ER_CANT_SET_GTID_NEXT_WHEN_OWNING_GTID`
--error ER_CANT_SET_GTID_NEXT_WHEN_OWNING_GTID
SET GTID_NEXT='AUTOMATIC';

# 5) Commit in order to release `GTID_NEXT`.
COMMIT;

--echo # TC.3 `GTID_NEXT` = `AUTOMATIC` + Query statement acquiring wr-lock
--echo # ------------------------------------------------------------------

# 1) SET `GTID_NEXT` to "AUTOMATIC".
SET GTID_NEXT='AUTOMATIC';

# 2) Execute an implicitly commiting statement.
CREATE TABLE t4(c1 FLOAT) ENGINE=InnoDB;

# 3) Execute `SELECT ... FOR UPDATE`.
--let $gtid_executed_before = `SELECT @@GTID_EXECUTED`
--let $gtid_owned_before = `SELECT @@GTID_OWNED`
SELECT * FROM t1, t3, t4 FOR UPDATE OF t1, t3, t4 SKIP LOCKED;

# 4) Assert that `GTID_OWNED` and `GTID_EXECUTED` remain unaltered.
--let $assert_text = No change in `GTID_EXECUTED`
--let $assert_cond = "[SELECT @@GTID_EXECUTED]" = "$gtid_executed_before"
--source include/assert.inc
--let $assert_text = No change in `GTID_OWNED`
--let $assert_cond = "[SELECT @@GTID_OWNED]" = "$gtid_owned_before"
--source include/assert.inc

# Clean up
DROP TABLE t1;
DROP TABLE t2;
DROP TABLE t3;
DROP TABLE t4;

RESET BINARY LOGS AND GTIDS;