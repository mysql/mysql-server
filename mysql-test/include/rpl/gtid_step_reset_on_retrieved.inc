# ==== Purpose ====
#
# Remember the current value of Retrieved_Gtid_Set
# to be used by subsequent calls to
# gtid_step_assert_on_retrieved.inc
#
# ==== Usage ====
#
# --source include/rpl/gtid_step_reset_on_retrieved.inc
#
# See gtid_step_assert_on_retrieved.inc for details and examples.

--let $include_filename= rpl/gtid_step_reset_on_retrieved.inc
--source include/begin_include_file.inc

--let $gtid_step_last= query_get_value(SHOW REPLICA STATUS, Retrieved_Gtid_Set, 1)

--let $include_filename= rpl/gtid_step_reset_on_retrieved.inc
--source include/end_include_file.inc
