--source include/check_ipv6.inc
--echo #
--echo # WL#12138 -- Add Admin Port
--echo #

--disable_query_log
CALL mtr.add_suppression("Failed to initialize TLS for channel: mysql_admin");
CALL mtr.add_suppression("Failed to set up SSL because of the following SSL library error");
--enable_query_log

--echo # Create the user u1
CREATE USER u1;
--echo # Grant the user u1 the privilege SERVICE_CONNECTION_ADMIN
GRANT SERVICE_CONNECTION_ADMIN ON *.* TO u1;

--echo # Create the user u2
CREATE USER u2;

--let $MYSQLD_LOG= $MYSQL_TMP_DIR/server.log
--let $MYSQLD_DATADIR= `SELECT @@datadir`

--echo # Create the user u3
CREATE USER u3;
--echo # Grant the user u3 the privilege SERVICE_CONNECTION_ADMIN
GRANT SUPER ON *.* TO u3;

--echo # Stop DB server which was created by MTR default
--source include/shutdown_mysqld.inc

--echo # Test case 1
--echo # Check that specially treated value :: is not allowed as part of
--echo # option admin-address.
--error 1
--exec $MYSQLD_CMD --log-error=$MYSQLD_LOG --datadir=$MYSQLD_DATADIR --secure-file-priv="" --admin-address=::

--let SEARCH_FILE=$MYSQLD_LOG
--let SEARCH_PATTERN=Invalid value for command line option admin-address: '::'
--source include/search_pattern.inc

--remove_file $MYSQLD_LOG

--echo # Test case 2
--echo # Check that specially treated value * is not allowed as part of
--echo # option admin-address.
--error 1
--exec $MYSQLD_CMD --log-error=$MYSQLD_LOG --datadir=$MYSQLD_DATADIR --secure-file-priv="" --admin-address=*

--let SEARCH_FILE=$MYSQLD_LOG
--let SEARCH_PATTERN=Invalid value for command line option admin-address: '*'
--source include/search_pattern.inc

--remove_file $MYSQLD_LOG

--echo # Test case 3
--echo # Check that specially treated value 0.0.0.0 is not allowed as part of
--echo # option admin-address.
--error 1
--exec $MYSQLD_CMD --log-error=$MYSQLD_LOG --datadir=$MYSQLD_DATADIR --secure-file-priv="" --admin-address=0.0.0.0

--let SEARCH_FILE=$MYSQLD_LOG
--let SEARCH_PATTERN=Invalid value for command line option admin-address: '0.0.0.0'
--source include/search_pattern.inc

--remove_file $MYSQLD_LOG

--echo # Test case 4
--echo # Check that non-existent host name specified as a value of the option admin-address results in error on server starting up.
--echo # option admin-address.
--error 1
--exec $MYSQLD_CMD --log-error=$MYSQLD_LOG --datadir=$MYSQLD_DATADIR --secure-file-priv="" --admin-address=non_existent_host

--let SEARCH_FILE=$MYSQLD_LOG
--let SEARCH_PATTERN=Can't start server: cannot resolve hostname
--source include/search_pattern.inc

--remove_file $MYSQLD_LOG

--echo # Starting up server with --admin-address=127.0.0.1 --max-connections=2
--force-cpdir $MYSQLD_DATADIR $MYSQL_TMP_DIR/newdd
--replace_result $MASTER_ADMINPORT ADMIN_PORT
--let $restart_parameters=restart: --admin-address=127.0.0.1 --admin-port=$MASTER_ADMINPORT --max-connections=2
--source include/start_mysqld.inc
--source include/xplugin_wait_for_interfaces.inc

--enable_connect_log
--echo # Test case 5
--echo # Check that ordinary connection using tcp protocol can be established

--connect(ordinary_tcp_con,localhost,root,,,,,TCP)
SELECT CURRENT_USER();

--disconnect ordinary_tcp_con
--source include/wait_until_disconnected.inc

--echo # Check error when starting a server having admin port which is busy
--error 1
--exec $MYSQLD_CMD --log-error=$MYSQLD_LOG --datadir=$MYSQL_TMP_DIR/newdd --secure-file-priv="" --admin-address=127.0.0.1 --admin-port=$MASTER_ADMINPORT

--let SEARCH_FILE=$MYSQLD_LOG
--let SEARCH_PATTERN=Can't start server: Bind on TCP/IP port: (Address .*in use|Only one usage of each socket address .* normally permitted)
--source include/search_pattern.inc

--remove_file $MYSQLD_LOG
--force-rmdir $MYSQL_TMP_DIR/newdd

--echo # Test case 6
--echo # Check that ordinary connection using default connection method can be established

--connect(ordinary_con,localhost,root,,,,,)
SELECT CURRENT_USER();

--disconnect ordinary_con
--source include/wait_until_disconnected.inc

--echo # Test case 7
--echo # Check that admin connection using tcp protocol can be established

--connect(admin_tcp_con,127.0.0.1,root,,,$MASTER_ADMINPORT,,TCP)
SELECT CURRENT_USER();

--echo # Check that a system message about the admin interface is written
--echo # to the error log
let $log_error_file= `SELECT @@GLOBAL.log_error`;
if($log_error_file == "stderr")
{
  let $log_error_file = $MYSQLTEST_VARDIR/log/mysqld.1.err;
}
--let SEARCH_FILE=$log_error_file
--let SEARCH_PATTERN=\[System\] \[MY\-[0-9]+\] \[Server\] Admin interface ready for connections, address: \'127.0.0.1\'  port: $MASTER_ADMINPORT
--replace_result $MASTER_ADMINPORT ADMIN_PORT
--source include/search_pattern.inc

--disconnect admin_tcp_con
--source include/wait_until_disconnected.inc

--echo # Test case 8
--echo # Check that the user u1 can establish connection to admin interface since
--echo # this user has the privilege SERVICE_CONNECTION_ADMIN
--connect(admin_tcp_con_u1,127.0.0.1,u1,,,$MASTER_ADMINPORT,,TCP)
SELECT CURRENT_USER();

--disconnect admin_tcp_con_u1
--source include/wait_until_disconnected.inc

--echo # Test case 9
--echo # Check that the user u2 can't establish connection to admin interface since
--echo # this user doesn't have the privilege SERVICE_CONNECTION_ADMIN
--connection default
--source include/count_sessions.inc
--replace_result $MASTER_MYSOCK MASTER_SOCKET $MASTER_ADMINPORT ADMIN_PORT
--error ER_SPECIFIC_ACCESS_DENIED_ERROR
--connect(admin_tcp_con_u2,127.0.0.1,u2,,,$MASTER_ADMINPORT,,TCP)

--echo # Check that the user u3 can't establish connection to admin interface since
--echo # this user doesn't have the privilege SERVICE_CONNECTION_ADMIN
--replace_result $MASTER_MYSOCK MASTER_SOCKET $MASTER_ADMINPORT ADMIN_PORT
--error ER_SPECIFIC_ACCESS_DENIED_ERROR
--connect(admin_tcp_con_u3,127.0.0.1,u3,,,$MASTER_ADMINPORT,,TCP)
--connection default

--echo # Wait until the failed connections are gone.
--source include/wait_until_count_sessions.inc

--echo # Test case 10
--echo # Check that the parameter max_connections does affect to an ordinary
--echo # connection and doesn't affect to a connection made to admin interface
--connect(ordinary_con_1,localhost,u1,,,,,)

--echo # Show how many active connections exist after the connection
--echo # ordinary_con_1 established
SHOW STATUS LIKE 'Threads_connected';

--echo # Since server was started with the option --max-connections=2 and
--echo # there are already two active connections (the first one is for
--echo # the default connection and the second one is for the connection
--echo # ordinary_con_1) an attempt to connect to the server results in
--echo # error ER_CON_COUNT_ERROR
--replace_result $MASTER_MYSOCK MASTER_SOCKET $MASTER_MYPORT MASTER_PORT
--error ER_CON_COUNT_ERROR
--connect(ordinary_con_2,localhost,u2,,,,,)

--echo # Check that attempt to establish the third connection to admin interface
--echo # doesn't result in error
--connect(admin_con_3,localhost,u1,,,$MASTER_ADMINPORT,,TCP)

--echo # Check that total number of concurrent connection made to admin interface
--echo # is not limited by the value max-connections + 1
--connect(admin_con_4,localhost,u1,,,$MASTER_ADMINPORT,,TCP)

--connection ordinary_con_1
--disconnect ordinary_con_1
--source include/wait_until_disconnected.inc

--connection admin_con_3
--disconnect admin_con_3
--source include/wait_until_disconnected.inc

--connection admin_con_4
--disconnect admin_con_4
--source include/wait_until_disconnected.inc

--connection default

--echo # Test case 11
--echo # Check that a dedicated thread for handling connection requests
--echo # on admin interface is not running in case a server started without
--echo # the option --create-admin-listener-thread=true
SELECT name, type FROM performance_schema.threads WHERE name = 'thread/sql/admin_interface';

--echo # Stop DB server
--source include/shutdown_mysqld.inc

--echo # Starting up server with --admin-address=127.0.0.1,
--echo # handle connections on admin interface by a dedicated thread
--let $restart_parameters=restart: --admin-address=127.0.0.1 --admin-port=$MASTER_ADMINPORT --create-admin-listener-thread=true
--replace_result $MASTER_ADMINPORT ADMIN_PORT
--source include/start_mysqld.inc

--echo # Test case 12
--echo # Check that a dedicated thread for handling connection requests on
--echo # admin interface is running in case a server started with the option
--echo # --create-admin-listener-thread=true
SELECT name, type FROM performance_schema.threads WHERE name = 'thread/sql/admin_interface';

--echo # Test case 13
--echo # Check that admin connection using tcp protocol can be established
--echo # when a server is started with option --create-admin-listener-thread=true

--connect(admin_tcp_con,127.0.0.1,root,,,$MASTER_ADMINPORT,,TCP)
SELECT CURRENT_USER();

--disconnect admin_tcp_con
--source include/wait_until_disconnected.inc

--echo # Test case 14
--echo # Check that admin interface is turned off
--echo # in case a server started with the --skip-networking option

--connection default
--echo # Stop DB server
--source include/shutdown_mysqld.inc

--echo # Starting up server with --admin-address=127.0.0.1 --skip-networking
--let $restart_parameters=restart: --admin-address=127.0.0.1 --admin-port=$MASTER_ADMINPORT --skip-networking
--let $wait_for_tcpsocket_status = no_wait
--replace_result $MASTER_ADMINPORT ADMIN_PORT
--source include/start_mysqld.inc

--replace_result $MASTER_MYSOCK MASTER_SOCKET $MASTER_ADMINPORT ADMIN_PORT
# Disable result_log to suppress output from subsequent connect() command
# since it outputs error code reasoning why connection wasn't established
# and its value could be distinct on different platforms.
--disable_result_log
--error CR_CONN_HOST_ERROR
--connect(admin_tcp_con,127.0.0.1,root,,,$MASTER_ADMINPORT,,TCP)

--echo # Test case 15
--echo # Check that admin interface is turned off in case a server started
--echo # with the --skip-grant-tables option. If the server is started with
--echo # the --skip-grant-tables option to disable authentication checks,
--echo # the server enables --skip-networking automatically to prevent remote
--echo # connections. Therefore, listening on admin interface must be disabled too.
--connection default
--echo # Stop DB server
--source include/shutdown_mysqld.inc

--echo # Starting up server with --admin-address=127.0.0.1 --skip-grant-tables
--let $restart_parameters=restart: --admin-address=127.0.0.1 --admin-port=$MASTER_ADMINPORT --skip-grant-tables
--let $wait_for_tcpsocket_status = no_wait
--replace_result $MASTER_ADMINPORT ADMIN_PORT
--source include/start_mysqld.inc

--replace_result $MASTER_MYSOCK MASTER_SOCKET $MASTER_ADMINPORT ADMIN_PORT
# Disable result_log to suppress output from subsequent connect() command
# since it outputs error code reasoning why connection wasn't established
# and its value could be distinct on different platforms.
--disable_result_log
--error CR_CONN_HOST_ERROR
--connect(admin_tcp_con,127.0.0.1,root,,,$MASTER_ADMINPORT,,TCP)

--connection default
--echo # Stop DB server
--source include/shutdown_mysqld.inc

--echo # Test case 16
--echo # Check that admin interface is not setup when the option
--echo # --admin-port is specified without --admin-address
--let $restart_parameters=restart: --admin-port=$MASTER_ADMINPORT
--replace_result $MASTER_ADMINPORT ADMIN_PORT
--source include/start_mysqld.inc

# Make ordinary connection
--connect(ordinary_tcp_con,localhost,root,,,,,TCP)

SELECT CURRENT_USER();
--replace_result $MASTER_ADMINPORT ADMIN_PORT
SELECT @@admin_address, @@admin_port;

--disconnect ordinary_tcp_con
--source include/wait_until_disconnected.inc

# Connection through admin port shouldn't be possible
# as --admin-address is not specified
--disable_result_log
--replace_result $MASTER_MYSOCK MASTER_SOCKET $MASTER_ADMINPORT ADMIN_PORT
--error CR_CONN_HOST_ERROR
--connect(admin_tcp_con,127.0.0.1,root,,,$MASTER_ADMINPORT,,TCP)
--enable_result_log

--echo # Test case 17
--echo # Check that we can connect to the server if admin
--echo # interface is set up on loopback IPV6 address
--connection default
--let $restart_parameters=restart: --skip-name-resolve --admin-address="::1" --admin-port=$MASTER_ADMINPORT
--replace_result $MASTER_ADMINPORT ADMIN_PORT
--source include/restart_mysqld.inc

--connect(admin_tcp_con_ipv6,::1,u1,,,$MASTER_ADMINPORT,,TCP)
--replace_result $MASTER_ADMINPORT ADMIN_PORT
SELECT @@admin_address, @@admin_port;

--disconnect admin_tcp_con_ipv6
--source include/wait_until_disconnected.inc

--connection default
--echo # Stop DB server
--source include/shutdown_mysqld.inc

--enable_result_log

--echo #
--echo # Starting mysqld in the regular mode...
--echo #
--let $restart_parameters=
--connection default

--source include/start_mysqld.inc

--echo # Cleaning up
DROP USER u1;
DROP USER u2;
DROP USER u3;
--disable_connect_log
