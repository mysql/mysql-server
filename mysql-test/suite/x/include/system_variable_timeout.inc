## Test body
##

--write_file $MYSQL_TMP_DIR/mysqlx-session-variable-timeout.xpl
-->import assert_variable.macro

-->newsession power_user	%POWER_USER%
-->stmtsql SET SESSION %TIMEOUT_VARIABLE%=10;
-->recvresult

-->callmacro Assert_session_variable	%TIMEOUT_VARIABLE%	10

-->newsession regular_user	%REGULAR_USER%
-->stmtsql SET SESSION %TIMEOUT_VARIABLE%=20;
-->recvresult

-->callmacro Assert_session_variable	%TIMEOUT_VARIABLE%	20

EOF

--echo #
--echo # Verify access to $timeout_variable system variable after plugin installation
--echo #
--echo # * invalid values
--echo # * user with permissions
--echo # * user without permissions
--echo #

--let $assert_text= $timeout_variable global variablem must exist
--let $assert_cond= [SELECT COUNT(@@GLOBAL.$timeout_variable) as Result;, Result, 1] =1
--source include/assert.inc

--error ER_WRONG_TYPE_FOR_VAR
eval SET @@GLOBAL.$timeout_variable="TEXT";
eval SELECT @@GLOBAL.$timeout_variable;
--let $assert_text= Invalid set doesn't change the value of  $timeout_variable (global variable)
--let $assert_cond= [SELECT @@GLOBAL.$timeout_variable = $timeout_default_value as Result;, Result, 1] =1
--source include/assert.inc

--echo #
--echo ## user with permissions
--connect(con_user2, 127.0.0.1, $user_with_permissions,'',,,,)

eval SET @@GLOBAL.$timeout_variable=10;

--let $assert_text= User with permissions can change $timeout_variable value
--let $assert_cond= [SELECT @@GLOBAL.$timeout_variable = 10 as Result;, Result, 1] =1
--source include/assert.inc

--echo #
--echo ## user without permissions
--connect(con_user1, 127.0.0.1, $user_without_permissions,'',,,,)

--error ER_SPECIFIC_ACCESS_DENIED_ERROR
eval SET @@GLOBAL.$timeout_variable=11;

--let $assert_text= User without permissions can't change $timeout_variable value
--let $assert_cond= [SELECT @@GLOBAL.$timeout_variable = 10 as Result;, Result, 1] =1
--source include/assert.inc

if ($is_session > 0)
{
  exec $MYSQLXTEST
    -u x_root
    -v%REGULAR_USER%=$user_without_permissions
    -v%POWER_USER%=$user_with_permissions
    -v%TIMEOUT_VARIABLE%=$timeout_variable
    --file $MYSQL_TMP_DIR/mysqlx-session-variable-timeout.xpl;
}

## Cleanup
##
--connection default
--disconnect con_user2
--disconnect con_user1

eval SET @@GLOBAL.$timeout_variable=DEFAULT;

--remove_file $MYSQL_TMP_DIR/mysqlx-session-variable-timeout.xpl
