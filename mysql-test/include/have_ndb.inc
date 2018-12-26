#
# Check if cluster is available by selecting from information_schema.engines
#

if (`SELECT COUNT(*) = 0 FROM INFORMATION_SCHEMA.ENGINES
       WHERE engine = 'ndbcluster' AND
             support IN ('YES', 'DEFAULT', 'ENABLED')`)
{
  --skip Test requires ndbcluster
}


