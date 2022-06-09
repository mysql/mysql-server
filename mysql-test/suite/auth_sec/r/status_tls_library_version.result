#
# WL#13407: A status variable for the openssl library version
#
# Success criteria: Should return 1 row
SELECT "PASSED" AS TEST_RESULT FROM performance_schema.global_status
WHERE VARIABLE_NAME='Tls_library_version' AND LENGTH(VARIABLE_VALUE) > 0;
TEST_RESULT
PASSED
# End of 8.0 tests
