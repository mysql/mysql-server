#
# Test account lock / unlock of the proxy and proxied users
#
--source include/have_plugin_auth.inc

--echo # Create proxy and proxied users
CREATE USER 'empl_external'@'localhost' IDENTIFIED WITH test_plugin_server AS 'employee';
CREATE USER 'employee'@'localhost' IDENTIFIED BY 'passkey';
GRANT PROXY ON 'employee'@'localhost' TO 'empl_external'@'localhost';

--echo # Lock proxied user - proxy user login should work
connection default;
ALTER USER employee@localhost ACCOUNT LOCK;
--connect(employee_con, localhost, empl_external, employee)
SELECT USER(), CURRENT_USER();
disconnect employee_con;

--echo # Lock proxy user - login should fail
connection default;
ALTER USER empl_external@localhost ACCOUNT LOCK;
--replace_result $MASTER_MYSOCK SOURCE_SOCKET $MASTER_MYPORT SOURCE_PORT
--error ER_ACCOUNT_HAS_BEEN_LOCKED
--connect(employee_con, localhost, empl_external, employee)

--echo # Unlocking proxied user should not change anything - login should fail
connection default;
ALTER USER employee@localhost ACCOUNT UNLOCK;
--replace_result $MASTER_MYSOCK SOURCE_SOCKET $MASTER_MYPORT SOURCE_PORT
--error ER_ACCOUNT_HAS_BEEN_LOCKED
--connect(employee_con, localhost, empl_external, employee)

#Cleanup
DROP USER 'empl_external'@'localhost', 'employee'@'localhost';