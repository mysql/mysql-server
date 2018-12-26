let $have_ndb_example= `SELECT LENGTH('$NDB_EXAMPLES_BINARY') > 0`;
if (!$have_ndb_example)
{
  --skip Test requires ndbapi examples binaries
}
