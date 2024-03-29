# ==== Purpose ====
#
# Used by scripts testing XA crash recovery, it searches
# `performance_schema.error_log` table for one occurrence of the given
# InnoDB message.
#
# ==== Parameters ====
#
#   $assert_select
#     Sub-string to search for in the error log.
#
# ==== References ====
#
# WL#11300: Crash-safe XA + binary log
#
# Related tests;
#   see extra/xa_crash_safe_tests/setup.inc
#

if ($assert_select == '') {
  --die ERROR IN TEST: You must set $assert_select before sourcing assert_recovery_message.inc
}

let $_row_count_to_assert = 
`SELECT COUNT(1) 
 FROM performance_schema.error_log 
 WHERE 
   subsystem = 'InnoDB' AND 
   prio = 'Note' AND 
   data LIKE '%$assert_select%'`;
--let $assert_text = Found log message(s) for "$assert_select"
--let $assert_cond = $_row_count_to_assert > 0
--source include/assert.inc
