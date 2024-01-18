# ==== Purpose ====
#
# Used by scripts testing XA crash recovery, it checks that the table used
# in those tests has a record count equal to the parameter.
#
# ==== Parameters ====
#
# $expected_row_count
#   The expected record count in the table used in XA crash safe tests.
#
# For other parameters, see `include/assert.inc`
#
# ==== References ====
#
#
# Related tests;
#   see common/xa_crash_safe/setup.inc
#

--let $count = `SELECT COUNT(1) FROM t1;`
--let $assert_text = Table 'test.t1' record count must be $expected_row_count
--let $assert_cond = $count = $expected_row_count
--source include/assert.inc
