## This is test file for connection authentication using mysqlx.
--source include/xplugin_create_user.inc
eval CREATE USER nonblank@localhost
            IDENTIFIED WITH $USER_AUTH_PLUGIN BY 'valid_password';

--write_file $MYSQL_TMP_DIR/mysqlx-connection_current_user.tmp
-->sql
select user(),current_user(),@@bind_address;
-->endsql
EOF

--write_file $MYSQL_TMP_DIR/mysqlx-connection_uninstallplugin.tmp
-->expecterror ER_PLUGGABLE_PROTOCOL_COMMAND_NOT_SUPPORTED
-->sql
uninstall plugin mysqlx;
-->endsql
EOF

--echo Test starts here

exec $MYSQLXTEST $USER_CIPHER
    --user nonblank
    --password='valid_password'
    --socket=$MASTER_X_MYSOCK
    --file=$MYSQL_TMP_DIR/mysqlx-connection_current_user.tmp 2>&1;

--echo
--echo #
--echo # Try login with valid user when server started with
--echo # skip-grant-tables and mysqlx loaded
--echo #

--replace_regex /\.dll/.so/
--let $wait_for_tcpsocket_status = undefined_value
--let $restart_parameters = restart: --skip-grant-tables
--source ../include/restart_mysqld_and_wait_for_xplugin.inc

--echo
--echo #
--echo # Connection should success with valid password
exec $MYSQLXTEST $USER_CIPHER
    --socket=$MASTER_X_MYSOCK
    --user nonblank
    --password='valid_password'
    --file=$MYSQL_TMP_DIR/mysqlx-connection_current_user.tmp 2>&1;

--echo
--echo #
--echo # Connection should success with invalid password
exec $MYSQLXTEST $USER_CIPHER
    --user nonblank
    --password='invalid'
    --socket=$MASTER_X_MYSOCK
    --file=$MYSQL_TMP_DIR/mysqlx-connection_current_user.tmp 2>&1;

--echo
--echo #
--echo # Uninstall mysqlx plugin when the server is started with
--echo # skip-grant-tables option
exec $MYSQLXTEST
    --socket=$MASTER_X_MYSOCK
    --user root
    --password=''
    --file=$MYSQL_TMP_DIR/mysqlx-connection_uninstallplugin.tmp 2>&1;

--echo
--echo #
--echo # Enable back the "grant-tables", mysqlx client
--echo # that uses invalid passord must be disconnected
FLUSH PRIVILEGES;
exec $MYSQLXTEST $USER_CIPHER
    --expect-error ER_ACCESS_DENIED_ERROR
    --socket=$MASTER_X_MYSOCK
    --user nonblank
    --password='invalid'
    --file=$MYSQL_TMP_DIR/mysqlx-connection_current_user.tmp 2>&1;

#
# Cleanup
#
--remove_file $MYSQL_TMP_DIR/mysqlx-connection_uninstallplugin.tmp
--remove_file $MYSQL_TMP_DIR/mysqlx-connection_current_user.tmp

--source include/xplugin_drop_user.inc
DROP USER nonblank@localhost;
