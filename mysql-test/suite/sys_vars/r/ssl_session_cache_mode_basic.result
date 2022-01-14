#
# WL#13075: Support TLS session reuse in the C API
#
# Check the default value
SELECT @@GLOBAL.ssl_session_cache_mode;
@@GLOBAL.ssl_session_cache_mode
1
# 1 expected
# Check if value can set
SET @@GLOBAL.ssl_session_cache_mode=false;
SELECT @@GLOBAL.ssl_session_cache_mode;
@@GLOBAL.ssl_session_cache_mode
0
# 0 expected
# Check if the value in GLOBAL Table matches value in variable
SELECT @@GLOBAL.ssl_session_cache_mode = VARIABLE_VALUE
FROM performance_schema.global_variables
WHERE VARIABLE_NAME='ssl_session_cache_mode';
@@GLOBAL.ssl_session_cache_mode = VARIABLE_VALUE
1
1 Expected
SELECT @@GLOBAL.ssl_session_cache_mode;
@@GLOBAL.ssl_session_cache_mode
0
SELECT COUNT(VARIABLE_VALUE)
FROM performance_schema.global_variables 
WHERE VARIABLE_NAME='ssl_session_cache_mode';
COUNT(VARIABLE_VALUE)
1
1 Expected
# Check if accessing variable with and without GLOBAL point to same variable
SELECT @@ssl_session_cache_mode = @@GLOBAL.ssl_session_cache_mode;
@@ssl_session_cache_mode = @@GLOBAL.ssl_session_cache_mode
1
# 1 expected
# Check if ssl_session_cache_mode can be accessed with the @@ sign
SELECT @@ssl_session_cache_mode;
@@ssl_session_cache_mode
0
SELECT @@local.ssl_session_cache_mode;
ERROR HY000: Variable 'ssl_session_cache_mode' is a GLOBAL variable
Expected error 'Variable is a GLOBAL variable'
SELECT @@SESSION.ssl_session_cache_mode;
ERROR HY000: Variable 'ssl_session_cache_mode' is a GLOBAL variable
Expected error 'Variable is a GLOBAL variable'
SELECT @@GLOBAL.ssl_session_cache_mode;
@@GLOBAL.ssl_session_cache_mode
0
0 Expected
SELECT ssl_session_cache_mode = @@SESSION.ssl_session_cache_mode;
ERROR HY000: Variable 'ssl_session_cache_mode' is a GLOBAL variable
# Set value to true
SET @@GLOBAL.ssl_session_cache_mode = true;
# Set value to false
SET @@GLOBAL.ssl_session_cache_mode = false;
# Set value to default
SET @@GLOBAL.ssl_session_cache_mode = default;
# Set value to foo
SET @@GLOBAL.ssl_session_cache_mode = 'foo';
ERROR 42000: Variable 'ssl_session_cache_mode' can't be set to the value of 'foo'
# Cleanup
SET @@GLOBAL.ssl_session_cache_mode = default;
