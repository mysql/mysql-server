#
# suite.pm will make sure that all tests including this file
# will be skipped unless innodb or xtradb is enabled
#
# The test below is redundant

if (`SELECT COUNT(*) = 0 FROM INFORMATION_SCHEMA.ENGINES WHERE engine = 'innodb' AND support IN ('YES', 'DEFAULT', 'ENABLED')`)
{
  --skip Test requires InnoDB.
}
