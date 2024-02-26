# ==== Purpose ====
#
# This include will try to issue the $statement after setting GTID_NEXT to the
# $next_gtid variable expecting $expected_error if it is defined and non-zero.
#
# If the $expected_error is defined and non-zero, this include will also
# rollback the statement to release the ownership of the previously set GTID.
#
# ==== Usage ====
#
# --let $statement= XXX
# --let $next_gtid= 'UUID:NUMBER'
# [--$expected_error= ER_XXX]
# --source extra/rpl_tests/rpl_gtid_drop_table.inc
#
# Parameters:
#   $statement
#     The SQL statement to be executed.
#
#   $next_gtid
#     The GTID to be set as GTID_NEXT.
#
#   $expected_error
#     The error expected when issuing the command.
#     Optional, not set means no expected error.

if (!$statement)
{
  --die ERROR IN TEST: the "statement" variable must be set
}

if (!$next_gtid)
{
  --die ERROR IN TEST: the "next_gtid" variable must be set
}

--eval SET GTID_NEXT= $next_gtid
if ($expected_error)
{
  --echo ## Expecting error $expected_error after the statement
  --error $expected_error
  --eval $statement
  ROLLBACK;
}
if (!$expected_error)
{
  --eval $statement
}
