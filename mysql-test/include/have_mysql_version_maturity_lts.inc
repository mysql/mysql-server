--source include/is_mysql_version_maturity_lts.inc
if (!$is_mysql_version_maturity_lts_out_var)
{
  --skip Test requires a server with LTS version
}
