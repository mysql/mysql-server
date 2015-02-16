# ==== Purpose ====
#
# Set GTID_NEXT to ANONYMOUS if GTID_MODE=OFF and UUID:NUMBER if
# GTID_MODE=ON.  Print the same output to the result log in both
# cases.
#
# ==== Usage ====
#
# [--let $gtid_next= AUTOMATIC|ANONYMOUS|GTID]
# --source include/set_gtid_next_gtid_mode_agnostic.inc
#
# Parameters:
#
#   $gtid_next
#     By default, uses ANONYMOUS if $gtid_mode=OFF or OFF_PERMISSIVE and
#     generates a new GTID if GTID_MODE=ON or ON_PERMISSIVE.
#     If this variable is set to AUTOMATIC or ANONYMOUS, uses that value;
#     if this variable is set to GTID, generates a new GTID.
#
#   $gtid_next_connection
#     By default, executes everything on the current connection.  When
#     generating a GTID, this includes functions and variables that
#     are unsafe for statement-based logging, so it will switch to row
#     format.  If $gtid_next_connection is set, these auxiliary unsafe
#     statements are executed on $gtid_next_connection instead.  The
#     SET statement is still executed on the current connection.
#
#   $gtid_next_mask_mode
#     By default, masks out the value of GTID_NEXT completely, like:
#       SET GTID_NEXT='#';
#     If this is set to 1, shows one of AUTOMATIC/ANONYMOUS/GTID, but
#     does not reveal the actual GTID.
#     If this is set to 2, shows the GTID number but not the UUID.
#     If this is set to 3, does not mask anything.

--let $_sgngma_mode= $gtid_next
if ($_sgngma_mode == '')
{
  --let $_sgngma_mode= ANONYMOUS
  if ($gtid_mode_on)
  {
    --let $_sgngma_mode= GTID
  }
}

if ($_sgngma_mode == GTID)
{
  if ($gtid_next_connection != '')
  {
    --let $_sgngma_connection= $CURRENT_CONNECTION
    --connection $gtid_next_connection
  }
  --let $_sgngma_value= `SELECT CONCAT(@@GLOBAL.SERVER_UUID, ':', GTID_NEXT_GENERATED(@@GLOBAL.GTID_EXECUTED, @@GLOBAL.SERVER_UUID))`
  if ($gtid_next_connection != '')
  {
    --connection $_sgngma_connection
  }
}
if ($_sgngma_mode != GTID)
{
  --let $_sgngma_value= $_sgngma_mode
}

if (!$gtid_next_mask_mode)
{
  --replace_result $_sgngma_value #
}
if ($gtid_next_mask_mode == 1)
{
  --replace_result $_sgngma_value $_sgngma_mode
}
if ($gtid_next_mask_mode == 2)
{
  --replace_result $_sgngma_uuid UUID
}
eval SET GTID_NEXT = '$_sgngma_value';
