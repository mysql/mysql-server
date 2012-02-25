# ==== Purpose ====
# 
# Ensure that the server is running with GTID support on.
#
# ==== Usage ====
#
# --source include/have_gtid.inc

--source include/have_log_bin.inc

if (`SELECT COUNT(*) = 0 OR VARIABLE_VALUE != 'ON' FROM INFORMATION_SCHEMA.GLOBAL_VARIABLES WHERE VARIABLE_NAME = 'GTID_MODE'`)
{
  --skip Test requires GTID_MODE=ON.
}
