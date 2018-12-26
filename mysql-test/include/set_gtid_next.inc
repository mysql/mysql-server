# ==== Purpose ====
#
# This is a simplified version of set_gtid_next__gtid_mode_agnostic.inc.
#
# It will set the GTID_NEXT session variable to ANONYMOUS if GTIDs
# are OFF or to SERVER_UUID:N (where N is the larger GTID executed plus one)
# if GTIDs are enabled.
#
# It is useful to test some functionalities regardless of GTIDs being enabled
# or not, when there is a need for non-automatic GTID_NEXT.
#
# It doesn't require gtid_utils.inc as set_gtid_next__gtid_mode_agnostic.inc.
#
# ==== Usage ====
#
# [--let $non_masked_gtid= 1]
# [--let $expected_error= error_number]
# --source include/set_gtid_next.inc
#
# Parameters:
#   $non_masked_gtids
#     By default, this include will mask the output of the statement in order
#     to produce the same output with or without GTIDs enabled. By setting this
#     parameter the include will not mask the output.
#
#   $expected_error
#     When set, the include will expect an error before setting the GTID_NEXT.
#

--let $_sgn_gtid_next= ANONYMOUS

if ( `SELECT @@GLOBAL.GTID_MODE = 'ON'` )
{
  # This routine assume that GTID_EXECUTED has only GTIDs from a single server
  --let $_sgn_colon= `SELECT LOCATE(':', @@GLOBAL.GTID_EXECUTED)`
  if ($_sgn_colon)
  {
    --let $_sgn_minus= `SELECT LOCATE('-', @@GLOBAL.GTID_EXECUTED, $_sgn_colon)`
    if ($_sgn_minus)
    {
      --let $_sgn_next= `SELECT SUBSTR(@@GLOBAL.GTID_EXECUTED, LOCATE('-', @@GLOBAL.GTID_EXECUTED, $_sgn_colon) + 1) + 1`
    }
    if (!$_sgn_minus)
    {
      --let $_sgn_next= `SELECT SUBSTR(@@GLOBAL.GTID_EXECUTED, $_sgn_colon + 1) + 1`
    }
  }
  if (!$_sgn_colon)
  {
    --let $_sgn_next= 1
  }
  --let $_sgn_server_uuid= `SELECT @@GLOBAL.SERVER_UUID`
  --let $_sgn_gtid_next= $_sgn_server_uuid:$_sgn_next
}

if (!$non_masked_gtid)
{
  --replace_result $_sgn_gtid_next #
}
if ($expected_error)
{
  --error $expected_error
}
--eval SET GTID_NEXT= '$_sgn_gtid_next'
