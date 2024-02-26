#
# WL#13075: Support TLS session reuse in the C API
#
# Check the default value
SELECT @@GLOBAL.ssl_session_cache_timeout;
@@GLOBAL.ssl_session_cache_timeout
300
300 Expected
# Check if value can set
SET @@GLOBAL.ssl_session_cache_timeout=400;
SELECT @@GLOBAL.ssl_session_cache_timeout;
@@GLOBAL.ssl_session_cache_timeout
400
400 expected
# Check if the value in GLOBAL Table matches value in variable
SELECT @@GLOBAL.ssl_session_cache_timeout = VARIABLE_VALUE
FROM performance_schema.global_variables
WHERE VARIABLE_NAME='ssl_session_cache_timeout';
@@GLOBAL.ssl_session_cache_timeout = VARIABLE_VALUE
1
1 Expected
SELECT @@GLOBAL.ssl_session_cache_timeout;
@@GLOBAL.ssl_session_cache_timeout
400
# Check if there's only one value in global_variables
SELECT COUNT(VARIABLE_VALUE)
FROM performance_schema.global_variables
WHERE VARIABLE_NAME='ssl_session_cache_timeout';
COUNT(VARIABLE_VALUE)
1
1 Expected
# Check if accessing variable with and without GLOBAL point to same variable
SELECT @@ssl_session_cache_timeout = @@GLOBAL.ssl_session_cache_timeout;
@@ssl_session_cache_timeout = @@GLOBAL.ssl_session_cache_timeout
1
# Check if ssl_session_cache_timeout can be accessed with @@ sign
SELECT @@ssl_session_cache_timeout;
@@ssl_session_cache_timeout
400
SELECT @@local.ssl_session_cache_timeout;
ERROR HY000: Variable 'ssl_session_cache_timeout' is a GLOBAL variable
Expected error 'Variable is a GLOBAL variable'
SELECT @@SESSION.ssl_session_cache_timeout;
ERROR HY000: Variable 'ssl_session_cache_timeout' is a GLOBAL variable
Expected error 'Variable is a GLOBAL variable'
SELECT @@GLOBAL.ssl_session_cache_timeout;
@@GLOBAL.ssl_session_cache_timeout
400
400 Expected
SELECT ssl_session_cache_timeout = @@SESSION.ssl_session_cache_timeout;
ERROR HY000: Variable 'ssl_session_cache_timeout' is a GLOBAL variable
# Set value to min
SET @@GLOBAL.ssl_session_cache_timeout = 0;
# Set value to max
SET @@GLOBAL.ssl_session_cache_timeout = 84600;
# Set value below min
SET @@GLOBAL.ssl_session_cache_timeout = -1;
Warnings:
Warning	1292	Truncated incorrect ssl_session_cache_timeout value: '-1'
# Set value above max
SET @@GLOBAL.ssl_session_cache_timeout = 84600 + 1;
Warnings:
Warning	1292	Truncated incorrect ssl_session_cache_timeout value: '84601'
# expect to be able to set it with step of 1
SET @@GLOBAL.ssl_session_cache_timeout = 300;
SET @@GLOBAL.ssl_session_cache_timeout = 301;
SET @@GLOBAL.ssl_session_cache_timeout = 299;
# Cleanup
SET @@GLOBAL.ssl_session_cache_timeout = default;
