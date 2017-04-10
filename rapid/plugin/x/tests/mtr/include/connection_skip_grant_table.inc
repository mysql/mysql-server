## This is test file for connection authentication using mysqlx.

eval CREATE USER nonblank@localhost
            IDENTIFIED WITH $USER_AUTH_PLUGIN BY 'nonblank';

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

--exec $MYSQLXTEST $USER_CIPHER -u nonblank --password='nonblank' --file=$MYSQL_TMP_DIR/mysqlx-connection_current_user.tmp 2>&1

--echo try login with valid user when server started with skip-grant-tables and mysqlx loaded
--replace_regex /\.dll/.so/
--let $restart_parameters = restart: --skip-grant-tables --plugin-load=$MYSQLXPLUGIN
--source ../include/restart_mysqld_and_wait_for_xplugin.inc
--echo Connection should success with valid password
--exec $MYSQLXTEST $USER_CIPHER -u nonblank --password='nonblank' --file=$MYSQL_TMP_DIR/mysqlx-connection_current_user.tmp 2>&1
--echo Connection should success with invalid password
--exec $MYSQLXTEST $USER_CIPHER -u nonblank --password='blank' --file=$MYSQL_TMP_DIR/mysqlx-connection_current_user.tmp 2>&1
FLUSH PRIVILEGES;

--let $expected_error_msg= Invalid user or password \(code 1045\)
--source ../include/mysqlxtest_expected_error.inc
--exec $MYSQLXTEST $USER_CIPHER -u nonblank --password='blank' --file=$MYSQL_TMP_DIR/mysqlx-connection_current_user.tmp 2>&1

##uninstall mysqlxplugin when server started with skip-grant-tables option and mysqlx loaded with super user
--exec $MYSQLXTEST -u root --password='' --file=$MYSQL_TMP_DIR/mysqlx-connection_uninstallplugin.tmp 2>&1

##cleanup
--remove_file $MYSQL_TMP_DIR/mysqlx-connection_uninstallplugin.tmp
--remove_file $MYSQL_TMP_DIR/mysqlx-connection_current_user.tmp

DROP USER nonblank@localhost;
