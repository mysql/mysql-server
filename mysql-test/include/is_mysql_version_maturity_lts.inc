# ==== Purpose ====
#
# Discover if server version maturity is LTS.
#
# Result is placed on the variable:
# - $is_mysql_version_maturity_lts_out_var
# which will be:
#   1  if the version is LTS
#   0  otherwise
#
# The current connection is preserved.
#
# ==== Usage ====
#
# --source include/is_mysql_version_maturity_lts.inc
#
# --echo MYSQL_VERSION_MATURITY_LTS: $is_mysql_version_maturity_lts_out_var

--let $is_mysql_version_maturity_lts_out_var= 0
if (`SELECT sys.version_major()=8 AND sys.version_minor()=4`)
{
  --let $is_mysql_version_maturity_lts_out_var= 1
}
