if (`select @@ndbinfo_version < ((7<<16) | (1 << 8))`)
{
  --disable_query_log
  DROP DATABASE ndbinfo;
  --enable_query_log
}
