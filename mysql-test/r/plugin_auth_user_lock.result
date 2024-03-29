# Create proxy and proxied users
CREATE USER 'empl_external'@'localhost' IDENTIFIED WITH test_plugin_server AS 'employee';
CREATE USER 'employee'@'localhost' IDENTIFIED BY 'passkey';
GRANT PROXY ON 'employee'@'localhost' TO 'empl_external'@'localhost';
# Lock proxied user - proxy user login should work
ALTER USER employee@localhost ACCOUNT LOCK;
SELECT USER(), CURRENT_USER();
USER()	CURRENT_USER()
empl_external@localhost	employee@localhost
# Lock proxy user - login should fail
ALTER USER empl_external@localhost ACCOUNT LOCK;
connect(localhost,empl_external,employee,test,MASTER_PORT,MASTER_SOCKET);
ERROR HY000: Access denied for user 'empl_external'@'localhost'. Account is locked.
# Unlocking proxied user should not change anything - login should fail
ALTER USER employee@localhost ACCOUNT UNLOCK;
connect(localhost,empl_external,employee,test,MASTER_PORT,MASTER_SOCKET);
ERROR HY000: Access denied for user 'empl_external'@'localhost'. Account is locked.
DROP USER 'empl_external'@'localhost', 'employee'@'localhost';
