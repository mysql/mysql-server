
## Test starts here
--let $mysqlxscript_success= $MYSQL_TMP_DIR/connection_mysqlx_user.xpl
--let $mysqlxscript_failure= $MYSQL_TMP_DIR/connection_mysqlx_user_failure.xpl
--let $MYSQLXTEST_LOGIN_SUCCESS= $MYSQLXTEST -uuser1 $USER_CIPHER -f $mysqlxscript_success --schema xtest --fatal-errors=1
--let $MYSQLXTEST_LOGIN_FAILURE= $MYSQLXTEST -uuser1 $USER_CIPHER -f $mysqlxscript_failure --schema xtest --fatal-errors=1 --no-auth

--write_file $mysqlxscript_success
-->stmtsql select current_user();
-->recvresult

-->noquery_result
-->stmtsql select * from `table_all_hosts`;
-->expecterror %TABLE_ALL_HOSTS_RESULT%
-->recvresult

-->stmtsql select * from `table_127.0.0.1`;
-->expecterror %TABLE_127.0.0.1_RESULT%
-->recvresult

-->stmtsql select * from `table_localhost`;
-->expecterror %TABLE_LOCALHOST_RESULT%
-->recvresult
EOF

--write_file $mysqlxscript_failure
-->echo Login with '%OPTION_CLIENT_USER%:%OPTION_CLIENT_PASSWORD%@%OPTION_CLIENT_HOST%/%OPTION_CLIENT_SCHEMA%' should fail
-->import connection.macro
-->callmacro Verify_its_xprotocol_connection
-->loginerror %EXPECTED_ERROR%	%OPTION_CLIENT_USER%	%OPTION_CLIENT_PASSWORD%	%OPTION_CLIENT_SCHEMA%
EOF

call mtr.add_suppression("Plugin mysqlx reported: .*Unsuccessful login attempt: Invalid user or password");

CREATE DATABASE xtest;
USE xtest;
eval CREATE USER user1@'%'         IDENTIFIED WITH $USER_AUTH_PLUGIN BY 'pass_all_hosts';
eval CREATE USER user1@'127.0.0.1' IDENTIFIED WITH $USER_AUTH_PLUGIN BY 'pass_127.0.0.1';
eval CREATE USER user1@'localhost' IDENTIFIED WITH $USER_AUTH_PLUGIN BY 'pass_localhost';

CREATE TABLE `table_all_hosts`(id integer);
CREATE TABLE `table_127.0.0.1`(id integer);
CREATE TABLE `table_localhost`(id integer);

INSERT INTO `table_all_hosts`(id) VALUES(2),(4);
INSERT INTO `table_127.0.0.1`(id) VALUES(1),(3);
INSERT INTO `table_localhost`(id) VALUES(5),(6);

GRANT SELECT ON xtest.`table_all_hosts` TO 'user1'@'%';
GRANT SELECT ON xtest.`table_127.0.0.1` TO 'user1'@'127.0.0.1';
GRANT SELECT ON xtest.`table_localhost` TO 'user1'@'localhost';

--echo ##
--echo ## X Plugin should only authenticate 'user1'@'127.0.0.1' account on TCP
--echo ##
exec $MYSQLXTEST_LOGIN_SUCCESS -p"pass_127.0.0.1"
                           -v%TABLE_ALL_HOSTS_RESULT%=0
                           -v%TABLE_127.0.0.1_RESULT%=0
                           -v%TABLE_LOCALHOST_RESULT%=0;
exec $MYSQLXTEST_LOGIN_FAILURE -p"pass_localhost" -v%EXPECTED_ERROR%=ER_ACCESS_DENIED_ERROR 2>&1;
exec $MYSQLXTEST_LOGIN_FAILURE -p"pass_all_hosts" -v%EXPECTED_ERROR%=ER_ACCESS_DENIED_ERROR 2>&1;

if ($UNIX_SOCKET_TESTING)
{
--echo ##
--echo ## X Plugin should only authenticate 'user1'@'localhost' account on UNIX socket
--echo ##
exec $MYSQLXTEST_LOGIN_SUCCESS -p"pass_localhost" --socket=$MASTER_X_MYSOCK
                           -v%TABLE_ALL_HOSTS_RESULT%=0
                           -v%TABLE_127.0.0.1_RESULT%=ER_TABLEACCESS_DENIED_ERROR
                           -v%TABLE_LOCALHOST_RESULT%=0;
exec $MYSQLXTEST_LOGIN_FAILURE -p"pass_127.0.0.1" --socket=$MASTER_X_MYSOCK -v%EXPECTED_ERROR%=ER_ACCESS_DENIED_ERROR 2>&1;
exec $MYSQLXTEST_LOGIN_FAILURE -p"pass_all_hosts" --socket=$MASTER_X_MYSOCK -v%EXPECTED_ERROR%=ER_ACCESS_DENIED_ERROR 2>&1;
}

DROP USER user1@'127.0.0.1';

--echo ##
--echo ## X Plugin should only authenticate 'user1'@'127.0.0.%' account on TCP
--echo ##
exec $MYSQLXTEST_LOGIN_FAILURE -p"pass_127.0.0.1" -v%EXPECTED_ERROR%=ER_ACCESS_DENIED_ERROR 2>&1;
exec $MYSQLXTEST_LOGIN_SUCCESS -p"pass_localhost"
                           -v%TABLE_ALL_HOSTS_RESULT%=0
                           -v%TABLE_127.0.0.1_RESULT%=ER_TABLEACCESS_DENIED_ERROR
                           -v%TABLE_LOCALHOST_RESULT%=0;
exec $MYSQLXTEST_LOGIN_FAILURE -p"pass_all_hosts" -v%EXPECTED_ERROR%=ER_ACCESS_DENIED_ERROR 2>&1;

DROP USER user1@'localhost';

--echo ##
--echo ## X Plugin should only authenticate 'user1'@'%' account on TCP
--echo ##
exec $MYSQLXTEST_LOGIN_FAILURE -p"pass_127.0.0.1" -v%EXPECTED_ERROR%=ER_ACCESS_DENIED_ERROR 2>&1;
exec $MYSQLXTEST_LOGIN_FAILURE -p"pass_localhost" -v%EXPECTED_ERROR%=ER_ACCESS_DENIED_ERROR 2>&1;
exec $MYSQLXTEST_LOGIN_SUCCESS -p"pass_all_hosts"
                           -v%TABLE_ALL_HOSTS_RESULT%=0
                           -v%TABLE_127.0.0.1_RESULT%=ER_TABLEACCESS_DENIED_ERROR
                           -v%TABLE_LOCALHOST_RESULT%=ER_TABLEACCESS_DENIED_ERROR;

## Postamble
DROP DATABASE xtest;

DROP USER user1@'%';

--remove_file $mysqlxscript_success
--remove_file $mysqlxscript_failure

