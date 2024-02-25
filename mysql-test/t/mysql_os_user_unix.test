--source include/not_windows.inc

--echo #
--echo # WL#12955: ADD OS USER AS CONNECTION ATTRIBUTE IN MYSQL CLIENT
--echo #  (The non-windows part)
--echo #

--echo # The environment variable is only read on non-windows platforms
exec unset SUDO_USER && $MYSQL test -e "SELECT COUNT(*) AS zero_sudo_count FROM performance_schema.session_connect_attrs WHERE attr_name = 'os_sudouser' AND PROCESSLIST_ID=CONNECTION_ID()";
exec SUDO_USER=gizmo $MYSQL test -e "SELECT attr_value AS must_be_gizmo FROM performance_schema.session_connect_attrs WHERE ATTR_NAME = 'os_sudouser'";


--echo # End of 8.0 tests
