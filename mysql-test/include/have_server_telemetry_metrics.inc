--disable_query_log
--disable_warnings
# Check if server telemetry metrics feature was compiled in,
# skip test if run on old mysqld or one compiled with -DDISABLE_PSI_METRICS
let $have_feat = `SELECT count(*)=1 AND variable_value="ON" AS enabled FROM performance_schema.global_status WHERE variable_name="Telemetry_metrics_supported";`;
if (!$have_feat)
{
  skip Requires server telemetry metrics support;
}
--enable_warnings
--enable_query_log
