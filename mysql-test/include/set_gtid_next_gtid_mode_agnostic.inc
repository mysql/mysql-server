# ==== Purpose ====
#
# Set GTID_NEXT to ANONYMOUS if GTID_MODE=OFF and UUID:NUMBER if
# GTID_MODE=ON.  Print the same output to the result log in both
# cases.
#
# ==== Usage ====
#
# --source include/set_gtid_next_gtid_mode_agnostic.inc

if ($gtid_mode_on)
{
  --let $_sgngma_value= `SELECT CONCAT(@@GLOBAL.SERVER_UUID, ':', GTID_NEXT_GENERATED(@@GLOBAL.GTID_EXECUTED, @@GLOBAL.SERVER_UUID))`
}
if (!$gtid_mode_on)
{
  --let $_sgngma_value= ANONYMOUS
}

--replace_result $_sgngma_value #
eval SET GTID_NEXT = '$_sgngma_value';
