** Setup **

SET @old_log_output=          @@global.log_output;
SET @old_general_log=         @@global.general_log;
SET @old_general_log_file=    @@global.general_log_file;
SET GLOBAL general_log_file = '.../log/proxy_general.log';
SET GLOBAL log_output =       'FILE,TABLE';
SET GLOBAL general_log=       'ON';
SET @default_mysql_native_password_proxy_users = @@global.mysql_native_password_proxy_users;
SET @default_check_proxy_users = @@global.check_proxy_users;
'#----- 2.1.12.1 Test of general log entries. -------------------#'
SET Global mysql_native_password_proxy_users=OFF;
** Creating new base user **
CREATE USER proxy_base@localhost;
GRANT ALTER ON *.* TO proxy_base@localhost;
** Creating new proxy user **
CREATE USER proxy_native@localhost IDENTIFIED WITH mysql_native_password;
GRANT CREATE ON *.* TO proxy_native@localhost;
GRANT PROXY ON proxy_base@localhost TO proxy_native@localhost;
SET Global mysql_native_password_proxy_users=ON;
** Connecting as proxy_native with proxy mapping disabled (native mapping on) **
SELECT CURRENT_USER(), USER(), @@session.proxy_user;
CURRENT_USER()	USER()	@@session.proxy_user
proxy_native@localhost	proxy_native@localhost	NULL
SHOW GRANTS;
Grants for proxy_native@localhost
GRANT CREATE ON *.* TO `proxy_native`@`localhost`
GRANT PROXY ON 'proxy_base'@'localhost' TO 'proxy_native'@'localhost'
** Connection default **
** Disconnecting connections **
SET Global check_proxy_users=ON;
** Connecting as proxy_native with proxy mapping enabled **
SELECT CURRENT_USER(), USER(), @@session.proxy_user;
CURRENT_USER()	USER()	@@session.proxy_user
proxy_base@localhost	proxy_native@localhost	'proxy_native'@'localhost'
SHOW GRANTS;
Grants for proxy_base@localhost
GRANT ALTER ON *.* TO `proxy_base`@`localhost`
** Disconnecting connections **
SET GLOBAL general_log=       'OFF';
CREATE TABLE test_log (argument TEXT);
LOAD DATA LOCAL INFILE '.../log/proxy_general.log'
     INTO TABLE test_log FIELDS TERMINATED BY '\n' LINES TERMINATED BY '\n';
SELECT count(argument) FROM test_log
WHERE argument LIKE '%proxy_native@localhost on test%';
count(argument)
1
SELECT count(argument) FROM test_log
WHERE argument LIKE '%proxy_native@localhost as proxy_base on test%';
count(argument)
1
DROP TABLE test_log;
DROP USER proxy_base@localhost;
DROP USER proxy_native@localhost;
SET @@global.mysql_native_password_proxy_users = @default_mysql_native_password_proxy_users;
SET @@global.check_proxy_users = @default_check_proxy_users;
SET GLOBAL general_log_file=  @old_general_log_file;
SET GLOBAL general_log=       @old_general_log;
SET GLOBAL log_output=        @old_log_output;
