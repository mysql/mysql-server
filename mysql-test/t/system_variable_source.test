########################################################################
# Test script to test system_variable_source service implementation.   #
########################################################################

CALL mtr.add_suppression("Failed to set up SSL because of the following *");

########################################################################
# INITIAL START:                                                       #
#    innodb_buffer_pool_size is set explicitly from mysqld_default.cnf #
#    Expected source : EXPLICIT                                        #
########################################################################
--echo # Install test component
INSTALL COMPONENT "file://component_test_system_variable_source";
--echo # Un-Install test component
UNINSTALL COMPONENT "file://component_test_system_variable_source";

-- echo # Print source value of innodb_buffer_pool_size
let $MYSQLD_DATADIR= `select @@datadir`;
cat_file $MYSQLD_DATADIR/test_system_variable_source.log;
remove_file $MYSQLD_DATADIR/test_system_variable_source.log;

########################################################################
# RESTART 1:                                                           #
#    innodb_buffer_pool_size is set from command line                  #
#    Expected source : COMMAND_LINE                                    #
########################################################################
let $restart_parameters = restart: --innodb_dedicated_server=OFF --innodb_buffer_pool_size=24M --skip-mysqlx;
--source include/restart_mysqld.inc

--echo # Install test component
INSTALL COMPONENT "file://component_test_system_variable_source";
--echo # Un-Install test component
UNINSTALL COMPONENT "file://component_test_system_variable_source";

-- echo # Print source value of innodb_buffer_pool_size
let $MYSQLD_DATADIR= `select @@datadir`;
cat_file $MYSQLD_DATADIR/test_system_variable_source.log;
remove_file $MYSQLD_DATADIR/test_system_variable_source.log;

########################################################################
#    innodb_buffer_pool_size is set dynamically                        #
#    Expected source : DYNAMIC                                         #
########################################################################
--disable_warnings
SET GLOBAL innodb_buffer_pool_size=134217728;
--enable_warnings

--echo # Install test component
INSTALL COMPONENT "file://component_test_system_variable_source";
--echo # Un-Install test component
UNINSTALL COMPONENT "file://component_test_system_variable_source";

-- echo # Print source value of innodb_buffer_pool_size
let $MYSQLD_DATADIR= `select @@datadir`;
cat_file $MYSQLD_DATADIR/test_system_variable_source.log;
remove_file $MYSQLD_DATADIR/test_system_variable_source.log;

########################################################################
# RESTART 2:                                                           #
#    innodb_buffer_pool_size is set from no where                      #
#    Expected source : COMPILED                                        #
########################################################################
# Set variables to be used in parameters of mysqld.
let $MYSQLD_DATADIR= `SELECT @@datadir`;
let $MYSQL_BASEDIR= `SELECT @@basedir`;
let $MYSQL_SOCKET= `SELECT @@socket`;
let $MYSQL_PIDFILE= `SELECT @@pid_file`;
let $MYSQL_PORT= `SELECT @@port`;
let $MYSQL_MESSAGESDIR= `SELECT @@lc_messages_dir`;
let $MYSQL_SERVER_ID= `SELECT @@server_id`;

--echo # Restart server with --no-defaults
--exec echo "wait" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
--shutdown_server
--source include/wait_until_disconnected.inc
--exec echo "restart:--no-defaults" --basedir=$MYSQL_BASEDIR --datadir=$MYSQLD_DATADIR --socket=$MYSQL_SOCKET --pid-file=$MYSQL_PIDFILE --port=$MYSQL_PORT --lc-messages-dir=$MYSQL_MESSAGESDIR --secure-file-priv="" --server-id=$MYSQL_SERVER_ID --innodb_dedicated_server=OFF --skip-mysqlx > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
--source include/wait_until_connected_again.inc

--echo # Install test component
INSTALL COMPONENT "file://component_test_system_variable_source";
--echo # Un-Install test component
UNINSTALL COMPONENT "file://component_test_system_variable_source";

-- echo # Print source value of innodb_buffer_pool_size
let $MYSQLD_DATADIR= `select @@datadir`;
cat_file $MYSQLD_DATADIR/test_system_variable_source.log;
remove_file $MYSQLD_DATADIR/test_system_variable_source.log;

# restore default values
let $restart_parameters = restart:;
--source include/restart_mysqld.inc
