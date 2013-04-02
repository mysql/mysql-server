# Skip the test if 'query cache' is disabled.
# Note : There are 2 techniques to disable query cache :
#        (1) Set query_cache_type to 0 or OFF
#        (2) Set query_cache_size to 0

disable_query_log;

let $do_skip=0;

let $qc_var_1= query_get_value("SELECT COUNT(*) AS var1 FROM INFORMATION_SCHEMA.SESSION_VARIABLES WHERE VARIABLE_NAME = 'have_query_cache' AND variable_value = 'YES'", var1, 1);
if ($qc_var_1 == 1)
{
  let $do_skip=1;
}

# Check if 'query_cache_type' is OFF
let $qc_var_2= query_get_value("SELECT COUNT(*) AS var2 FROM INFORMATION_SCHEMA.SESSION_VARIABLES WHERE VARIABLE_NAME = 'query_cache_type' AND VARIABLE_VALUE = 'OFF'", var2, 1);
if ($qc_var_2 == 1)
{
  let $do_skip=0;
}

# Check if 'query_cache_size' is 0
let $qc_var_3= query_get_value("SELECT COUNT(*) AS var3 FROM INFORMATION_SCHEMA.SESSION_VARIABLES WHERE VARIABLE_NAME = 'query_cache_size' AND VARIABLE_VALUE = 0", var3, 1);

if ($qc_var_3 == 1)
{
  let $do_skip=0;
}

if ($do_skip)
{
  skip Query Cache enabled, skipping test;
}

enable_query_log;
