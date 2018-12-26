# Skip the test in case the default storage engine is NDB.

if (`SELECT CONVERT(@@default_storage_engine USING latin1) IN ('ndbcluster','NDBCLUSTER')`)
{
  --skip Test requires default storage engine (@@default_storage_engine) != NDB
}
