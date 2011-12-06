# ==== Purpose ====
# 
# Ensure that the server is running with GTID support compiled in.
#
# ==== Usage ====
#
# --source include/have_gtid.inc

if (`SELECT COUNT(*) = 0 OR VARIABLE_VALUE != 'YES' FROM INFORMATION_SCHEMA.GLOBAL_VARIABLES WHERE VARIABLE_NAME = 'HAVE_GTID'`)
{
  --skip Test requires GTID.
}
