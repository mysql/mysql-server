# ==== Purpose ====
#
# Test that GTID_NEXT can be set to a given value, assert that
# GTID_OWNED and GTID_NEXT have the correct values afterwards.
#
# ==== Usage ====
#
# --let $gtid_next= VALUE
# --let $old_gtid_next= VALUE
# [--let $error= ERROR]
# --source extra/rpl_tests/rpl_gtid_mode_set_gtid_next.inc
#
# Parameters:
#   $gtid_next
#     The new value of GTID_NEXT
#
#   $old_gtid_next
#     The old value of GTID_NEXT. This is only used for output.
#
#   $error
#     If set, an error will be expected.
#
# Side effects:
# - Sets $old_gtid_next to $gtid_next.
#
# ==== References ====
#
# - Used in rpl_gtid_mode.test
# - Introduced in WL#7083.

# Remember old value of gtid_next
--let $old_gtid_next= `SELECT @@SESSION.GTID_NEXT`

# Set gtid_next.
--echo # $old_gtid_next -> $gtid_next
if ($error)
{
  --echo Expecting error $error.
  --error $error
  # Due to bugs in mtr this is needed.
  --disable_abort_on_error
}
eval SET GTID_NEXT = '$gtid_next';

# Assert that gtid_next changed accordingly.
if ($error)
{
  # On error gtid_next should not change
  --let $gtid_next= $old_gtid_next
  # This is needed to match the disable_abort_on_error above.
  --enable_abort_on_error
}
--let $actual_gtid_next= `SELECT @@SESSION.GTID_NEXT`
--let $assert_cond= "$actual_gtid_next" = "$gtid_next"
--let $assert_text= GTID_NEXT == "$gtid_next"
--source include/assert.inc

# Assert that gtid_owned changed accordingly.
--let $gtid_owned= $gtid_next
if ($gtid_next == 'AUTOMATIC')
{
  # Nothing should be owned when gtid_next=automatic
  --let $gtid_owned=
}
if ($error)
{
  # Nothing should change after an error.
  --let $gtid_owned=
}
--let $actual_gtid_owned= `SELECT @@SESSION.GTID_OWNED`
--let $assert_cond= "$actual_gtid_owned" = "$gtid_owned"
--let $assert_text= GTID_OWNED == "$gtid_owned"
--source include/assert.inc

# Rollback to release ownership.
# Don't rollback in case error is expected.
if (!$error)
{
  ROLLBACK;
}
#--let $old_gtid_next= $gtid_next
