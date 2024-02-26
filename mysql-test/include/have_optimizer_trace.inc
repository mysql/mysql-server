--disable_query_log
--disable_warnings
# Check if variable optimizer_trace exists
let $have_var = `select /*+SET_VAR(use_secondary_engine=OFF)*/ TRUE from performance_schema.session_variables where variable_name="optimizer_trace"`;
if (!$have_var)
{
  skip requires optimizer trace support;
}
--enable_warnings
--enable_query_log
