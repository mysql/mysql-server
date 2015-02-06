# ==== Purpose ====
# 
# Ensure that the server is running with GTID support on.
#
# ==== Usage ====
#
# --source include/have_gtid.inc

--source include/have_log_bin.inc

--disable_warnings
if (`SELECT COUNT(*) = 0 FROM INFORMATION_SCHEMA.GLOBAL_VARIABLES WHERE VARIABLE_NAME = 'GTID_MODE' AND VARIABLE_VALUE = 'ON'`)
{
  --skip Test requires GTID_MODE=ON.
}
--enable_warnings
