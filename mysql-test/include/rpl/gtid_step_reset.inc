# ==== Purpose ====
#
# Remember the current value of @@GLOBAL.GTID_EXECUTED to be used by
# subsequent calls to gtid_step_assert.inc
#
# ==== Usage ====
#
# --source include/rpl/gtid_step_reset.inc
#
# See gtid_step_assert.inc for details and examples.

--let $include_filename= rpl/gtid_step_reset.inc
--source include/begin_include_file.inc

--let $gtid_step_last= `SELECT @@GLOBAL.GTID_EXECUTED`

--let $include_filename= rpl/gtid_step_reset.inc
--source include/end_include_file.inc
