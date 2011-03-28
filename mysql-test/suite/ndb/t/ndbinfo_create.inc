if (`select @@ndbinfo_version < ((7<<16) | (1 << 8))`)
{
  # Check if ndbinfo.sql exists
  --source have_ndbinfo.inc

  # Run the ndbinfo.sql script that creates ndbinfo database, tables and views
  --disable_warnings
  --disable_query_log
  --disable_result_log
  set @ndbinfo_skip_version_check=1;
  source $NDBINFO_SQL;
  --enable_result_log
  --enable_query_log
  --enable_warnings
}