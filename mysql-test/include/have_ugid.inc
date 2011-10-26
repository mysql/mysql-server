# ==== Purpose ====
# 
# Ensure that the server is running with UGID support compiled in.
#
# ==== Usage ====
#
# --source include/have_ugid.inc

if (`SELECT COUNT(*) = 0 OR VARIABLE_VALUE != 'YES' FROM INFORMATION_SCHEMA.GLOBAL_VARIABLES WHERE VARIABLE_NAME = 'HAVE_UGID'`)
{
  --skip Test requires UGID.
}
