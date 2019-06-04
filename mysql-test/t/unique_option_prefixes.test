
--echo #
--echo # WL#6951: Deprecate unique option prefixes
--echo #

--echo # Test unique option prefixes in client

--echo # should fail due to wrong option
--disable_result_log
--error 7
--exec $MYSQL test --init-comman="CREATE TABLE wl6951(a int)" -e "SELECT 1 AS CMD" 2>&1
--enable_result_log

--echo # table must not exist
--error ER_NO_SUCH_TABLE
SELECT * FROM wl6951;
