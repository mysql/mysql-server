# ==== Purpose ====
# 
# Ensure that the server is running with GTID support off.
#
# ==== Usage ====
#
# --source include/not_gtid_enabled.inc

--disable_warnings
if (`SELECT COUNT(*) = 0 FROM performance_schema.global_variables WHERE VARIABLE_NAME = 'GTID_MODE' AND VARIABLE_VALUE = 'OFF'`)
{
  --skip Test requires GTIDs off.
}
--enable_warnings
