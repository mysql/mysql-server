--disable_query_log
--disable_warnings
# Make sure no telemetry component is already installed.
let $have_component = `SELECT count(*)>=1 AS installed FROM performance_schema.global_variables WHERE variable_name like "%telemetry%";`;
if ($have_component)
{
  skip Requires no telemetry component already installed;
}
--enable_warnings
--enable_query_log
