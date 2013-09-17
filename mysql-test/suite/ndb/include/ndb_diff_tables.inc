# Avoid direct dependency on diff_tables.inc

# Check $ndb_diff_tables parameter
if (!$ndb_diff_tables)
{
  die Missing parameter $ndb_diff_tables to ndb_diff_tables.inc;
}

let $diff_tables = $ndb_diff_tables;
let $_save_include_silent = $include_silent;
let $include_silent = 1;
source include/diff_tables.inc;
let $include_silent = $_save_include_silent;

# Reset input parameters to detect missing arguments
let $ndb_diff_tables =;
