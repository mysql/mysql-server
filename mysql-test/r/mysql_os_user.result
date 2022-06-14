#
# WL#12955: ADD OS USER AS CONNECTION ATTRIBUTE IN MYSQL CLIENT
#
must_be_non_zero
1
SELECT COUNT(*) AS must_be_zero_for_test FROM performance_schema.session_connect_attrs WHERE attr_name IN ('os_user', 'os_sudouser') AND PROCESSLIST_ID=CONNECTION_ID();
must_be_zero_for_test
0
# End of 8.0 tests
