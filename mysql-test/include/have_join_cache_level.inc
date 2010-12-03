--disable_query_log

# Check if variable optimizer_join_cache_level exists
let $have_var = `select TRUE from information_schema.session_variables where variable_name="optimizer_join_cache_level"`;
if (!$have_var)
{
  skip Variable optimizer_join_cache_level is not supported;
}

--disable_warnings
let $save = `SELECT @@optimizer_join_cache_level`;
--eval set @@optimizer_join_cache_level = $requires_join_cache_level;
let $ok = `SELECT @@optimizer_join_cache_level = $requires_join_cache_level`;
--eval SET @@optimizer_join_cache_level = $save;
--enable_warnings
--enable_query_log

if (!$ok)
{
  skip Unsupported setting for optimizer_join_cache_level;
}
