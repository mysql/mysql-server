# ==== Purpose ====
# 
# Ensure that the server is running with GTID support off.
#
# ==== Usage ====
#
# --source include/not_gtid_enabled.inc

if (`SELECT COUNT(*) = 0 OR VARIABLE_VALUE != 'OFF' FROM INFORMATION_SCHEMA.GLOBAL_VARIABLES WHERE VARIABLE_NAME = 'GTID_MODE'`)
{
  --skip Test requires GTIDs off.
}
