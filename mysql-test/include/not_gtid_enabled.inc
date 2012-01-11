# ==== Purpose ====
# 
# Ensure that the server is running with GTID support off.
#
# ==== Usage ====
#
# --source include/have_gtid.inc

if (`SELECT COUNT(*) = 0 OR VARIABLE_VALUE != 'OFF' FROM INFORMATION_SCHEMA.GLOBAL_VARIABLES WHERE VARIABLE_NAME = 'GTID_MODE'`)
{
  --skip Test requires GTIDs off.
}
