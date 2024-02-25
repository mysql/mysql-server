# Check that ndb_mgm has been compiled with support for debugging commands

disable_query_log;

source include/check_ndb_debug.inc;

if (!$have_ndb_debug)
{
  --skip Test requires debug compiled NDB
}

enable_query_log;
