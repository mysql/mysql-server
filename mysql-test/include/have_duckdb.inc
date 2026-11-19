if (!`SELECT COUNT(*) FROM information_schema.plugins WHERE plugin_name = 'duckdb'`)
{
  --skip Requires the DuckDB storage engine
}
