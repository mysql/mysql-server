#
# Check if myisam is available by selecting from is.engines
# else skip the test, however do not change the default.
#

if (`SELECT COUNT(*) = 0 FROM INFORMATION_SCHEMA.ENGINES WHERE engine = 'myisam' AND support IN ('YES', 'DEFAULT', 'ENABLED')`)
{
  --skip Test requires MyISAM.
}
