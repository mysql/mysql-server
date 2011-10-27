--disable_query_log

# Check if variable optimizer_trace exists
let $have_var = `select TRUE from information_schema.session_variables where variable_name="optimizer_trace"`;
if (!$have_var)
{
  skip requires optimizer trace support;
}
--enable_query_log
