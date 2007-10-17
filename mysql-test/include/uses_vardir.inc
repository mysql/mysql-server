#
# Some tests uses LOAD DATA with a relative path
# and need to see for example ../std_data
#
# Also if an absolute path was used, the server might be started
# with --secure-file-priv and wouldn't be allowed to LOAD a file
# outside of it's vardir anyway
#


if (`select LOCATE('$MYSQLTEST_VARDIR', REPLACE(@@datadir, '\\\\', '/')) != 1`)
{
  skip Need mysqld in MYSQLTEST_VARDIR;
}
