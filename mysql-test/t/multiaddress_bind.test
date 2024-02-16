--echo #
--echo # WL#11652 -- Support multiple addresses for the --bind-address command option
--echo #

--source include/check_ipv6.inc

## Setting --bind-address=127.0.0.1,::1 need to be done here, inside
## the test after the check for IPv6.  If it is done in -master.opt,
## MTR will attempt to start the server with an IPv6 address, and the
## server start will fail on machines without IPv6.

--let $restart_parameters=restart: --bind-address=127.0.0.1,::1
--source include/restart_mysqld.inc

--let $MYSQLD_LOG= $MYSQL_TMP_DIR/server.log
--let $MYSQLD_DATADIR= `SELECT @@datadir`
--let DEFARGS= --no-defaults --log-error=$MYSQLD_LOG --datadir=$MYSQLD_DATADIR --secure-file-priv="" --socket=$MYSQLD_SOCKET --tls-version= --skip-mysqlx

--echo #
--echo # Server is started with --bind-address=127.0.0.1,::1
--echo # Check that server accepts incoming connection both
--echo # on the address 127.0.0.1 and on the address ::1
--echo #
--echo # Connecting to a server via 127.0.0.1
--connect(con1,127.0.0.1,root,,,,,TCP)

--echo # Connecting to a server via ::1
--connect(con2,::1,root,,,,,TCP)

--connection con1
--disconnect con1

--connection con2
--disconnect con2

--connection default
SELECT @@global.bind_address;

--echo # Stop DB server which was created by MTR default
--exec echo "wait" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
--source include/shutdown_mysqld.inc

--echo # Check that not existing IP address as a value of
--echo # the option --bind-address is treated as an error.
--error 1
--exec $MYSQLD $DEFARGS --bind-address=128.0.0.1

--let SEARCH_FILE=$MYSQLD_LOG
--let SEARCH_PATTERN= MY-010262.*Bind on TCP/IP port:.*
--source include/search_pattern.inc

--remove_file $MYSQLD_LOG

--echo # Check that not existing IP address as one of a value of
--echo # the option --bind-address is treated as an error.
--error 1
--exec $MYSQLD $DEFARGS --bind-address=128.0.0.1,::1

--let SEARCH_FILE=$MYSQLD_LOG
--let SEARCH_PATTERN= MY-010262.*Bind on TCP/IP port:.*
--source include/search_pattern.inc

--remove_file $MYSQLD_LOG

--echo #
--echo # Starting mysqld in the regular mode...
--echo #
--let $restart_parameters =restart: --bind-address=127.0.0.1
--source include/start_mysqld.inc
SELECT @@global.bind_address;

--echo # Stop DB server which was created by MTR default
--exec echo "wait" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
--source include/shutdown_mysqld.inc

--echo #
--echo # Starting mysqld in the regular mode...
--echo #
--let $restart_parameters =restart: --bind-address=*
--source include/start_mysqld.inc
SELECT @@global.bind_address;

--echo # Stop DB server which was created by MTR default
--exec echo "wait" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
--source include/shutdown_mysqld.inc

--echo #
--echo # Starting mysqld in the regular mode...
--echo #
--let $restart_parameters =restart: --bind-address=::
--source include/start_mysqld.inc
SELECT @@global.bind_address;

--echo # Stop DB server which was created by MTR default
--exec echo "wait" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
--source include/shutdown_mysqld.inc

--echo # Check separators not being ','.
--error 1
--exec $MYSQLD $DEFARGS --bind-address=127.0.0.1 ::1

--let SEARCH_FILE=$MYSQLD_LOG
--let SEARCH_PATTERN= Invalid value for command line option bind-addresses:

--remove_file $MYSQLD_LOG

--echo # Check that specially treated value :: is not allowed as part of
--echo # multi-value option bind-address.
--error 1
--exec $MYSQLD $DEFARGS --bind-address=127.0.0.1,::1,::

--let SEARCH_FILE=$MYSQLD_LOG
--let SEARCH_PATTERN= Invalid value for command line option bind-addresses:
--source include/search_pattern.inc

--remove_file $MYSQLD_LOG

--echo # Check that specially treated value * is not allowed as part of
--echo # multi-value option bind-address.
--error 1
--exec $MYSQLD $DEFARGS --bind-address=127.0.0.1,::1,*

--let SEARCH_FILE=$MYSQLD_LOG
--let SEARCH_PATTERN= Invalid value for command line option bind-addresses:
--source include/search_pattern.inc

--remove_file $MYSQLD_LOG

--echo # Check that server catches a parsing error during processing of
--echo # multi-value option bind-address.
--echo # Two adjacent commas in a value of the option --bind-address is treated
--echo # as an error.
--error 1
--exec $MYSQLD $DEFARGS --bind-address=127.0.0.1,,::1

--let SEARCH_FILE=$MYSQLD_LOG
--let SEARCH_PATTERN= Invalid value for command line option bind-addresses:
--source include/search_pattern.inc

--remove_file $MYSQLD_LOG

--echo # Check that a server catches a parsing error during processing of
--echo # multi-value option bind-address.
--echo # Comma in the end of a value of the option --bind-address is treated
--echo # as an error.
--error 1
--exec $MYSQLD $DEFARGS --bind-address=127.0.0.1,::1,

--let SEARCH_FILE=$MYSQLD_LOG
--let SEARCH_PATTERN= Invalid value for command line option bind-addresses:
--source include/search_pattern.inc

--remove_file $MYSQLD_LOG

--echo # Check that a server catches a parsing error during processing of
--echo # multi-value option bind-address.
--echo # Comma in the begining of a value of the option --bind-address
--echo # is treated as an error.
--error 1
--exec $MYSQLD $DEFARGS --bind-address=,127.0.0.1,::1

--let SEARCH_FILE=$MYSQLD_LOG
--let SEARCH_PATTERN= Invalid value for command line option bind-addresses:
--source include/search_pattern.inc

--remove_file $MYSQLD_LOG

--echo # Check that a server catches a parsing error during processing of
--echo # multi-value option bind-address.
--echo # empty value of the option --bind-address
--echo # is treated as an error.
--error 1
--exec $MYSQLD $DEFARGS --bind-address=

--let SEARCH_FILE=$MYSQLD_LOG
--let SEARCH_PATTERN= Invalid value for command line option bind-addresses:
--source include/search_pattern.inc

--remove_file $MYSQLD_LOG

--echo # Check that a server catches a parsing error during processing of
--echo # multi-value option bind-address.

--echo # Check that specially treated ipv4 address 0.0.0.0 is not allowed
--echo # as part of multi-value option bind-address.
--error 1
--exec $MYSQLD $DEFARGS --bind-address=0.0.0.0,::1

--let SEARCH_FILE=$MYSQLD_LOG
--let SEARCH_PATTERN= Invalid value for command line option bind-addresses:
--source include/search_pattern.inc

--remove_file $MYSQLD_LOG

--echo # Check that a server catches a parsing error during processing of
--echo # multi-value option bind-address.
--echo # empty value of the option --bind-address
--echo # is treated as an error.
--error 1
--exec $MYSQLD $DEFARGS --bind-address=

--let SEARCH_FILE=$MYSQLD_LOG
--let SEARCH_PATTERN= Invalid value for command line option bind-addresses:
--source include/search_pattern.inc

--remove_file $MYSQLD_LOG

--echo #
--echo # Starting mysqld in the regular mode...
--echo #
--let $restart_parameters =

--source include/start_mysqld.inc
