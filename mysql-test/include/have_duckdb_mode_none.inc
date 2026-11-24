let $have_duckdb_mode_none= 1;
if (`SELECT COUNT(*) FROM information_schema.plugins WHERE plugin_name = 'duckdb'`)
{
  let $have_duckdb_mode_none= `SELECT @@duckdb_mode LIKE 'NONE'`;
}
if (!$have_duckdb_mode_none)
{
  --skip Test requires duckdb_mode = NONE
}
