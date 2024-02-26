# ==== Purpose ====
#
# Used by scripts testing XA crash recovery, it checks the line count
# returnes by `XA RECOVER` against the given count parameter and, if
# different from zero, tries to find the given XID in the list.
#
# ==== Parameters ====
#
# $expected_prepared_xa_count
#   The expected line count return by `XA RECOVER`
#
# $xid_data
#   The XID raw data, to find in the `XA RECOVER` returned list. If
#   `$expected_prepared_xa_count` is non-zero, will try to assert that the
#   provided XID exists in the `XA RECOVER` outputted list. Support for
#   more than one XID to test isn't provided.
#
# ==== References ====
#
# Related tests:
#   see extra/xa_crash_safe_tests/setup.inc
#

--let $statement = XA RECOVER
--let $column = data
--source include/get_row_count.inc

if ($expected_prepared_xa_count == 0) {
  --let $assert_text = No XA transactions in PREPARED state
}
if ($expected_prepared_xa_count != 0) {
  --let $assert_text = $expected_prepared_xa_count XA transaction(s) in PREPARED state
}
# Test `XA RECOVER` returned line count
--let $assert_cond = "$row_count" = "$expected_prepared_xa_count"
--source include/assert.inc

if ($expected_prepared_xa_count != 0) {
  if ($xid_data != '') {
    # For each line returned from `XA RECOVER`, try to find the given XID
    --let $row_number = $row_count
    while($row_number != 0) {
      --let $pending_xa = query_get_value(XA RECOVER, data, $row_number)
      if ($pending_xa != $xid_data) {
        --let $pending_xa =
        --dec $row_number
      }
      if ($pending_xa == $xid_data) {
        # If found, exit loop
        --let $row_number = 0
      }
    }
    --let $assert_text = XA transaction $xid is in PREPARED state
    --let $assert_cond = "$pending_xa" = "$xid_data"
    --source include/assert.inc
  }
}
