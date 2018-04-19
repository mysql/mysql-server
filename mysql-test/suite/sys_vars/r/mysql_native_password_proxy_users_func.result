** Setup **

SET @default_mysql_native_password_proxy_users = @@global.mysql_native_password_proxy_users;
SET @default_check_proxy_users = @@global.check_proxy_users;
'#----- 2.2.4 With super privilege ------------------------------#'
SET Global mysql_native_password_proxy_users=OFF;
'#----- 2.2.4 Without super privilege ---------------------------#'
** Creating new user with out super privilege**
CREATE USER sameea;
** Connecting connn using username 'sameea' **
SET GLOBAL mysql_native_password_proxy_users=ON;
ERROR 42000: Access denied; you need (at least one of) the SUPER or SYSTEM_VARIABLES_ADMIN privilege(s) for this operation
SET @@global.mysql_native_password_proxy_users=ON;
ERROR 42000: Access denied; you need (at least one of) the SUPER or SYSTEM_VARIABLES_ADMIN privilege(s) for this operation
** Connection default **
** Closing connection **
'#----- 2.2.2.2 OFF no assignment of proxy user -----------------#'
SET Global mysql_native_password_proxy_users=OFF;
SELECT @@global.check_proxy_users;
@@global.check_proxy_users
0
** Creating new base user **
CREATE USER ''@localhost;
'#----- 4.4) Test to ensure users not defined with any PROXY privileges log in normally.-#';
CREATE USER proxy_base@localhost;
GRANT ALTER ON *.* TO proxy_base@localhost;
** Creating new proxy user **
CREATE USER proxy_native@localhost IDENTIFIED WITH mysql_native_password;
GRANT CREATE ON *.* TO proxy_native@localhost;
GRANT PROXY ON proxy_base@localhost TO proxy_native@localhost;
** Creating new proxy user with missing base user **
CREATE USER proxy_none@localhost IDENTIFIED WITH mysql_native_password;
GRANT PROXY ON proxy_blah@localhost TO proxy_none@localhost;
'#----- 4.3)Test to ensure proxy privileges are not chained. ----------------------------#';
CREATE USER proxy_base_multi@localhost;
GRANT SELECT ON *.* TO proxy_base_multi@localhost;
** Creating new proxy user **
CREATE USER proxy_native_1@localhost IDENTIFIED WITH mysql_native_password;
GRANT DELETE ON *.* TO proxy_native_1@localhost;
CREATE USER proxy_native_2@localhost IDENTIFIED WITH mysql_native_password;
GRANT INSERT,DELETE ON *.* TO proxy_native_2@localhost;
CREATE USER proxy_native_3@localhost IDENTIFIED WITH mysql_native_password;
GRANT SELECT,DELETE ON *.* TO proxy_native_3@localhost;
GRANT PROXY ON proxy_base_multi@localhost TO proxy_native_1@localhost;
GRANT PROXY ON proxy_base_multi@localhost TO proxy_native_2@localhost;
GRANT PROXY ON proxy_base_multi@localhost TO proxy_native_3@localhost;
** Creating new proxy user **
CREATE USER proxy_native_4@localhost IDENTIFIED WITH mysql_native_password;
CREATE USER proxy_native_41@localhost IDENTIFIED WITH mysql_native_password;
GRANT PROXY ON proxy_base@localhost TO proxy_native_4@localhost;
GRANT PROXY ON proxy_base_multi@localhost TO proxy_native_4@localhost;
GRANT INSERT ON *.* TO proxy_native_4@localhost;
GRANT PROXY ON proxy_native_4@localhost TO proxy_native_41@localhost;
GRANT UPDATE ON *.* TO proxy_native_41@localhost;
'#----- 4.2.check_proxy_users=OFF: mysql_native_password_proxy_users=OFF -----#';
** Connecting as proxy_native with proxy mapping disabled **
SELECT CURRENT_USER(), USER(), @@session.proxy_user;
CURRENT_USER()	USER()	@@session.proxy_user
proxy_native@localhost	proxy_native@localhost	NULL
SHOW GRANTS;
Grants for proxy_native@localhost
GRANT CREATE ON *.* TO `proxy_native`@`localhost`
GRANT PROXY ON 'proxy_base'@'localhost' TO 'proxy_native'@'localhost'
** Connection default **
SET PASSWORD FOR 'proxy_native'@'localhost' = 'testpw';
** Connecting as proxy_native,password with proxy mapping disabled **
SELECT CURRENT_USER(), USER(), @@session.proxy_user;
CURRENT_USER()	USER()	@@session.proxy_user
proxy_native@localhost	proxy_native@localhost	NULL
SHOW GRANTS;
Grants for proxy_native@localhost
GRANT CREATE ON *.* TO `proxy_native`@`localhost`
GRANT PROXY ON 'proxy_base'@'localhost' TO 'proxy_native'@'localhost'
** Connection default **
SET PASSWORD FOR 'proxy_native'@'localhost' = '';
** Connecting as proxy_none with proxy mapping disabled **
SELECT CURRENT_USER(), USER(), @@session.proxy_user;
CURRENT_USER()	USER()	@@session.proxy_user
proxy_none@localhost	proxy_none@localhost	NULL
SHOW GRANTS;
Grants for proxy_none@localhost
GRANT USAGE ON *.* TO `proxy_none`@`localhost`
GRANT PROXY ON 'proxy_blah'@'localhost' TO 'proxy_none'@'localhost'
** Connecting as proxy_native_1 with proxy mapping disabled **
SELECT CURRENT_USER(), USER(), @@session.proxy_user;
CURRENT_USER()	USER()	@@session.proxy_user
proxy_native_1@localhost	proxy_native_1@localhost	NULL
SHOW GRANTS;
Grants for proxy_native_1@localhost
GRANT DELETE ON *.* TO `proxy_native_1`@`localhost`
GRANT PROXY ON 'proxy_base_multi'@'localhost' TO 'proxy_native_1'@'localhost'
** Connecting as proxy_native_2 with proxy mapping disabled **
SELECT CURRENT_USER(), USER(), @@session.proxy_user;
CURRENT_USER()	USER()	@@session.proxy_user
proxy_native_2@localhost	proxy_native_2@localhost	NULL
SHOW GRANTS;
Grants for proxy_native_2@localhost
GRANT INSERT, DELETE ON *.* TO `proxy_native_2`@`localhost`
GRANT PROXY ON 'proxy_base_multi'@'localhost' TO 'proxy_native_2'@'localhost'
** Connecting as proxy_native_4 with proxy mapping disabled **
SELECT CURRENT_USER(), USER(), @@session.proxy_user;
CURRENT_USER()	USER()	@@session.proxy_user
proxy_native_4@localhost	proxy_native_4@localhost	NULL
SHOW GRANTS;
Grants for proxy_native_4@localhost
GRANT INSERT ON *.* TO `proxy_native_4`@`localhost`
GRANT PROXY ON 'proxy_base'@'localhost' TO 'proxy_native_4'@'localhost'
GRANT PROXY ON 'proxy_base_multi'@'localhost' TO 'proxy_native_4'@'localhost'
** Connection default **
** Disconnecting connections **
'#----- 2.2.2.1 ON assignment of proxy user ------------------#'
'#----- 4.2.check_proxy_users=OFF: mysql_native_password_proxy_users=ON ------#';
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
SET PASSWORD FOR 'proxy_native'@'localhost' = 'testpw';
** Connecting as proxy_native,password with proxy mapping disabled (native mapping on) **
SELECT CURRENT_USER(), USER(), @@session.proxy_user;
CURRENT_USER()	USER()	@@session.proxy_user
proxy_native@localhost	proxy_native@localhost	NULL
SHOW GRANTS;
Grants for proxy_native@localhost
GRANT CREATE ON *.* TO `proxy_native`@`localhost`
GRANT PROXY ON 'proxy_base'@'localhost' TO 'proxy_native'@'localhost'
** Connection default **
SET PASSWORD FOR 'proxy_native'@'localhost' = '';
** Connecting as proxy_none with proxy mapping disabled (native mapping on) **
SELECT CURRENT_USER(), USER(), @@session.proxy_user;
CURRENT_USER()	USER()	@@session.proxy_user
proxy_none@localhost	proxy_none@localhost	NULL
SHOW GRANTS;
Grants for proxy_none@localhost
GRANT USAGE ON *.* TO `proxy_none`@`localhost`
GRANT PROXY ON 'proxy_blah'@'localhost' TO 'proxy_none'@'localhost'
** Connecting as proxy_native_1 with proxy mapping disabled (native mapping on)**
SELECT CURRENT_USER(), USER(), @@session.proxy_user;
CURRENT_USER()	USER()	@@session.proxy_user
proxy_native_1@localhost	proxy_native_1@localhost	NULL
SHOW GRANTS;
Grants for proxy_native_1@localhost
GRANT DELETE ON *.* TO `proxy_native_1`@`localhost`
GRANT PROXY ON 'proxy_base_multi'@'localhost' TO 'proxy_native_1'@'localhost'
** Connecting as proxy_native_2 with proxy mapping disabled (native mapping on) **
SELECT CURRENT_USER(), USER(), @@session.proxy_user;
CURRENT_USER()	USER()	@@session.proxy_user
proxy_native_2@localhost	proxy_native_2@localhost	NULL
SHOW GRANTS;
Grants for proxy_native_2@localhost
GRANT INSERT, DELETE ON *.* TO `proxy_native_2`@`localhost`
GRANT PROXY ON 'proxy_base_multi'@'localhost' TO 'proxy_native_2'@'localhost'
** Connecting as proxy_native_4 with proxy mapping disabled (native mapping on) **
SELECT CURRENT_USER(), USER(), @@session.proxy_user;
CURRENT_USER()	USER()	@@session.proxy_user
proxy_native_4@localhost	proxy_native_4@localhost	NULL
SHOW GRANTS;
Grants for proxy_native_4@localhost
GRANT INSERT ON *.* TO `proxy_native_4`@`localhost`
GRANT PROXY ON 'proxy_base'@'localhost' TO 'proxy_native_4'@'localhost'
GRANT PROXY ON 'proxy_base_multi'@'localhost' TO 'proxy_native_4'@'localhost'
** Connecting as chained proxy_native_41 with proxy mapping disabled (native mapping on) **
SELECT CURRENT_USER(), USER(), @@session.proxy_user;
CURRENT_USER()	USER()	@@session.proxy_user
proxy_native_41@localhost	proxy_native_41@localhost	NULL
SHOW GRANTS;
Grants for proxy_native_41@localhost
GRANT UPDATE ON *.* TO `proxy_native_41`@`localhost`
GRANT PROXY ON 'proxy_native_4'@'localhost' TO 'proxy_native_41'@'localhost'
** Connection default **
GRANT PROXY ON ''@localhost TO proxy_native_1@localhost;
** Connecting as Anonymus with proxy mapping disabled (native mapping on) **
SELECT @@session.proxy_user IS NOT NULL;
@@session.proxy_user IS NOT NULL
0
** Connection default **
REVOKE PROXY ON ''@localhost FROM proxy_native_1@localhost;
** Connection default **
** Disconnecting connections **
'#----- 1.2.2.1 check and native ON --------------------------#'
'#----- 4.2.check_proxy_users=ON: mysql_native_password_proxy_users=ON ------#';
SET Global check_proxy_users=ON;
** Connecting as proxy_native with proxy mapping enabled **
SELECT CURRENT_USER(), USER(), @@session.proxy_user;
CURRENT_USER()	USER()	@@session.proxy_user
proxy_base@localhost	proxy_native@localhost	'proxy_native'@'localhost'
SHOW GRANTS;
Grants for proxy_base@localhost
GRANT ALTER ON *.* TO `proxy_base`@`localhost`
** Connection default **
SET PASSWORD FOR 'proxy_native'@'localhost' = 'testpw';
** Connecting as proxy_native,password with proxy mapping enabled **
SELECT CURRENT_USER(), USER(), @@session.proxy_user;
CURRENT_USER()	USER()	@@session.proxy_user
proxy_base@localhost	proxy_native@localhost	'proxy_native'@'localhost'
SHOW GRANTS;
Grants for proxy_base@localhost
GRANT ALTER ON *.* TO `proxy_base`@`localhost`
** Connection default **
SET PASSWORD FOR 'proxy_native'@'localhost' = '';
** Connecting as proxy_none with proxy mapping enabled **
SELECT CURRENT_USER(), USER(), @@session.proxy_user;
CURRENT_USER()	USER()	@@session.proxy_user
proxy_none@localhost	proxy_none@localhost	NULL
SHOW GRANTS;
Grants for proxy_none@localhost
GRANT USAGE ON *.* TO `proxy_none`@`localhost`
GRANT PROXY ON 'proxy_blah'@'localhost' TO 'proxy_none'@'localhost'
** Connecting as proxy_native_1 with proxy mapping enabled **
SELECT CURRENT_USER(), USER(), @@session.proxy_user;
CURRENT_USER()	USER()	@@session.proxy_user
proxy_base_multi@localhost	proxy_native_1@localhost	'proxy_native_1'@'localhost'
SHOW GRANTS;
Grants for proxy_base_multi@localhost
GRANT SELECT ON *.* TO `proxy_base_multi`@`localhost`
** Connecting as proxy_native_2 with proxy mapping enabled **
SELECT CURRENT_USER(), USER(), @@session.proxy_user;
CURRENT_USER()	USER()	@@session.proxy_user
proxy_base_multi@localhost	proxy_native_2@localhost	'proxy_native_2'@'localhost'
SHOW GRANTS;
Grants for proxy_base_multi@localhost
GRANT SELECT ON *.* TO `proxy_base_multi`@`localhost`
** Connecting as proxy_native_4 with proxy mapping enabled **
SELECT CURRENT_USER() IN ('proxy_base@localhost','proxy_base_multi@localhost');
CURRENT_USER() IN ('proxy_base@localhost','proxy_base_multi@localhost')
1
SELECT USER(), @@session.proxy_user;
USER()	@@session.proxy_user
proxy_native_4@localhost	'proxy_native_4'@'localhost'
SHOW GRANTS;
Grants for proxy_base@localhost
GRANT ALTER ON *.* TO `proxy_base`@`localhost`
** Connecting as chained proxy_native_41 with proxy mapping enabled **
SELECT CURRENT_USER() IN ('proxy_base@localhost','proxy_base_multi@localhost');
CURRENT_USER() IN ('proxy_base@localhost','proxy_base_multi@localhost')
0
SELECT CURRENT_USER() IN ('proxy_native_4@localhost');
CURRENT_USER() IN ('proxy_native_4@localhost')
1
SELECT USER(), @@session.proxy_user;
USER()	@@session.proxy_user
proxy_native_41@localhost	'proxy_native_41'@'localhost'
SHOW GRANTS;
Grants for proxy_native_4@localhost
GRANT INSERT ON *.* TO `proxy_native_4`@`localhost`
GRANT PROXY ON 'proxy_base'@'localhost' TO 'proxy_native_4'@'localhost'
GRANT PROXY ON 'proxy_base_multi'@'localhost' TO 'proxy_native_4'@'localhost'
** Connection default **
GRANT PROXY ON ''@localhost TO proxy_native_1@localhost;
** Connecting as Anonymus with proxy mapping enabled **
SELECT @@session.proxy_user IS NOT NULL;
@@session.proxy_user IS NOT NULL
0
** Connection default **
REVOKE PROXY ON ''@localhost FROM proxy_native_1@localhost;
** Disconnecting connections **
DROP USER ''@localhost;
DROP USER proxy_base@localhost;
DROP USER proxy_base_multi@localhost;
DROP USER proxy_native@localhost;
DROP USER proxy_none@localhost;
DROP USER proxy_native_1@localhost;
DROP USER proxy_native_2@localhost;
DROP USER proxy_native_3@localhost;
DROP USER proxy_native_4@localhost;
DROP USER proxy_native_41@localhost;
DROP USER sameea;
SET @@global.mysql_native_password_proxy_users = @default_mysql_native_password_proxy_users;
SET @@global.check_proxy_users = @default_check_proxy_users;
