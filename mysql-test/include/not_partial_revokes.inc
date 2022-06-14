# ==== Purpose ====
#
# Ensure that the server is running with partial_revokes off
#
# ==== Usage ====
#
# source include/not_partial_revokes.inc;

--let $_not_partial_revokes= query_get_value(SHOW GLOBAL VARIABLES LIKE 'partial_revokes', Value, 1)
if ($_not_partial_revokes != OFF)
{
  --skip Test requires --partial_revokes=OFF
}
