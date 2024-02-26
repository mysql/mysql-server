--echo #
--echo # WL#12955: ADD OS USER AS CONNECTION ATTRIBUTE IN MYSQL CLIENT
--echo #

exec $MYSQL test -e "SELECT length(attr_value) > 0 AS must_be_non_zero FROM performance_schema.session_connect_attrs WHERE attr_name = 'os_user' AND PROCESSLIST_ID=CONNECTION_ID()";
SELECT COUNT(*) AS must_be_zero_for_test FROM performance_schema.session_connect_attrs WHERE attr_name IN ('os_user', 'os_sudouser') AND PROCESSLIST_ID=CONNECTION_ID();

--echo # End of 8.0 tests
