# ==== Purpose ====
#
# Used by scripts testing XA crash recovery, it searches
# `performance_schema.error_log` table for one occurrence of the given
# message.
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
#   see common/xa_crash_safe/setup.inc
#

if ($assert_select == '') {
  --die ERROR IN TEST: You must set $assert_select before sourcing assert_recovery_message.inc
}

let $_row_count_to_assert =
`SELECT COUNT(1)
 FROM performance_schema.error_log
 WHERE
   (subsystem = 'Server' OR
    subsystem = 'InnoDB') AND
   prio = 'Note' AND
   REGEXP_LIKE(data, '.*$assert_select.*') = 1`;
--let $assert_text = Found 1 log message(s) for "$assert_select"
--let $assert_cond = $_row_count_to_assert = 1
--source include/assert.inc
