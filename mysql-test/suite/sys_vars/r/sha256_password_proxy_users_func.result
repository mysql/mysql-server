** Setup **

SET @default_sha256_password_proxy_users = @@global.sha256_password_proxy_users;
SET @default_check_proxy_users = @@global.check_proxy_users;
'#----- 3.2.4 With super privileges --------------------------#'
SET Global sha256_password_proxy_users=OFF;
'#----- 3.2.4 Without super privileges ------------------------#'
** Creating new user with out super privilege**
CREATE USER sameea;
** Connecting connn using username 'sameea' **
SET GLOBAL sha256_password_proxy_users=ON;
ERROR 42000: Access denied; you need (at least one of) the SUPER or SYSTEM_VARIABLES_ADMIN privilege(s) for this operation
SET @@global.sha256_password_proxy_users=ON;
ERROR 42000: Access denied; you need (at least one of) the SUPER or SYSTEM_VARIABLES_ADMIN privilege(s) for this operation
** Connection default **
** Closing connection **
'#----- 3.2.2.2 check OFF and sha256 OFF -------------------#'
SET Global sha256_password_proxy_users=OFF;
SELECT @@global.check_proxy_users;
@@global.check_proxy_users
0
** Creating new base user **
CREATE USER ''@localhost;
'#----- 4.4) Test to ensure users not defined with any PROXY privileges log in normally.-#';
CREATE USER proxy_base@localhost;
GRANT ALTER ON *.* TO proxy_base@localhost;
** Creating new proxy user **
CREATE USER proxy_sha256@localhost IDENTIFIED WITH sha256_password;
GRANT CREATE ON *.* TO proxy_base@localhost;
GRANT PROXY ON proxy_base@localhost TO proxy_sha256@localhost;
** Creating new proxy user with missing base user **
CREATE USER proxy_none@localhost IDENTIFIED WITH sha256_password;
GRANT PROXY ON proxy_blah@localhost TO proxy_none@localhost;
'#----- 4.3)Test to ensure proxy privileges are not chained. ----------------------------#';
CREATE USER proxy_base_multi@localhost;
GRANT SELECT ON *.* TO proxy_base_multi@localhost;
** Creating new proxy user **
CREATE USER proxy_sha256_1@localhost IDENTIFIED WITH sha256_password;
GRANT DELETE ON *.* TO proxy_sha256_1@localhost;
CREATE USER proxy_sha256_2@localhost IDENTIFIED WITH sha256_password;
GRANT INSERT,DELETE ON *.* TO proxy_sha256_2@localhost;
CREATE USER proxy_sha256_3@localhost IDENTIFIED WITH sha256_password;
GRANT SELECT,DELETE ON *.* TO proxy_sha256_3@localhost;
GRANT PROXY ON proxy_base_multi@localhost TO proxy_sha256_1@localhost;
GRANT PROXY ON proxy_base_multi@localhost TO proxy_sha256_2@localhost;
GRANT PROXY ON proxy_base_multi@localhost TO proxy_sha256_3@localhost;
** Creating new proxy user **
CREATE USER proxy_sha256_4@localhost IDENTIFIED WITH sha256_password;
CREATE USER proxy_sha256_41@localhost IDENTIFIED WITH sha256_password;
GRANT PROXY ON proxy_base@localhost TO proxy_sha256_4@localhost;
GRANT PROXY ON proxy_base_multi@localhost TO proxy_sha256_4@localhost;
GRANT INSERT ON *.* TO proxy_sha256_4@localhost;
GRANT PROXY ON proxy_sha256_4@localhost TO proxy_sha256_41@localhost;
GRANT UPDATE ON *.* TO proxy_sha256_41@localhost;
'#----- 4.2.check_proxy_users=OFF: sha256_password_proxy_users=OFF -----#';
** Connecting as proxy_sha256 with proxy mapping disabled **
SELECT CURRENT_USER(), USER(), @@session.proxy_user;
CURRENT_USER()	USER()	@@session.proxy_user
proxy_sha256@localhost	proxy_sha256@localhost	NULL
SHOW GRANTS;
Grants for proxy_sha256@localhost
GRANT USAGE ON *.* TO `proxy_sha256`@`localhost`
GRANT PROXY ON 'proxy_base'@'localhost' TO 'proxy_sha256'@'localhost'
** Connection default **
ALTER USER 'proxy_sha256'@'localhost' IDENTIFIED BY 'testpw';
** Connecting as proxy_sha256, password with proxy mapping disabled **
SELECT CURRENT_USER(), USER(), @@session.proxy_user;
CURRENT_USER()	USER()	@@session.proxy_user
proxy_sha256@localhost	proxy_sha256@localhost	NULL
SHOW GRANTS;
Grants for proxy_sha256@localhost
GRANT USAGE ON *.* TO `proxy_sha256`@`localhost`
GRANT PROXY ON 'proxy_base'@'localhost' TO 'proxy_sha256'@'localhost'
** Connection default **
ALTER USER 'proxy_sha256'@'localhost' IDENTIFIED BY '';
** Connecting as proxy_none with proxy mapping disabled **
SELECT CURRENT_USER(), USER(), @@session.proxy_user;
CURRENT_USER()	USER()	@@session.proxy_user
proxy_none@localhost	proxy_none@localhost	NULL
SHOW GRANTS;
Grants for proxy_none@localhost
GRANT USAGE ON *.* TO `proxy_none`@`localhost`
GRANT PROXY ON 'proxy_blah'@'localhost' TO 'proxy_none'@'localhost'
** Connecting as proxy_sha256_1 with proxy mapping disabled **
SELECT CURRENT_USER(), USER(), @@session.proxy_user;
CURRENT_USER()	USER()	@@session.proxy_user
proxy_sha256_1@localhost	proxy_sha256_1@localhost	NULL
SHOW GRANTS;
Grants for proxy_sha256_1@localhost
GRANT DELETE ON *.* TO `proxy_sha256_1`@`localhost`
GRANT PROXY ON 'proxy_base_multi'@'localhost' TO 'proxy_sha256_1'@'localhost'
** Connecting as proxy_sha256_2 with proxy mapping disabled **
SELECT CURRENT_USER(), USER(), @@session.proxy_user;
CURRENT_USER()	USER()	@@session.proxy_user
proxy_sha256_2@localhost	proxy_sha256_2@localhost	NULL
SHOW GRANTS;
Grants for proxy_sha256_2@localhost
GRANT INSERT, DELETE ON *.* TO `proxy_sha256_2`@`localhost`
GRANT PROXY ON 'proxy_base_multi'@'localhost' TO 'proxy_sha256_2'@'localhost'
** Connecting as proxy_sha256_4 with proxy mapping disabled **
SELECT CURRENT_USER(), USER(), @@session.proxy_user;
CURRENT_USER()	USER()	@@session.proxy_user
proxy_sha256_4@localhost	proxy_sha256_4@localhost	NULL
SHOW GRANTS;
Grants for proxy_sha256_4@localhost
GRANT INSERT ON *.* TO `proxy_sha256_4`@`localhost`
GRANT PROXY ON 'proxy_base'@'localhost' TO 'proxy_sha256_4'@'localhost'
GRANT PROXY ON 'proxy_base_multi'@'localhost' TO 'proxy_sha256_4'@'localhost'
** Connection default **
** Disconnecting connections **
'#----- 3.2.2.1 check OFF and sha256 ON ----------------------#'
'#----- 4.2.check_proxy_users=OFF: sha256_password_proxy_users=ON ------#';
SET Global sha256_password_proxy_users=ON;
** Connecting as proxy_sha256 with proxy mapping disabled (sha256 mapping on) **
SELECT CURRENT_USER(), USER(), @@session.proxy_user;
CURRENT_USER()	USER()	@@session.proxy_user
proxy_sha256@localhost	proxy_sha256@localhost	NULL
SHOW GRANTS;
Grants for proxy_sha256@localhost
GRANT USAGE ON *.* TO `proxy_sha256`@`localhost`
GRANT PROXY ON 'proxy_base'@'localhost' TO 'proxy_sha256'@'localhost'
** Connection default **
ALTER USER 'proxy_sha256'@'localhost' IDENTIFIED BY 'testpw';
** Connecting as proxy_sha256, password with proxy mapping disabled (sha256 mapping on) **
SELECT CURRENT_USER(), USER(), @@session.proxy_user;
CURRENT_USER()	USER()	@@session.proxy_user
proxy_sha256@localhost	proxy_sha256@localhost	NULL
SHOW GRANTS;
Grants for proxy_sha256@localhost
GRANT USAGE ON *.* TO `proxy_sha256`@`localhost`
GRANT PROXY ON 'proxy_base'@'localhost' TO 'proxy_sha256'@'localhost'
** Connection default **
ALTER USER 'proxy_sha256'@'localhost' IDENTIFIED BY '';
** Connecting as proxy_none with proxy mapping disabled (sha256 mapping on) **
SELECT CURRENT_USER(), USER(), @@session.proxy_user;
CURRENT_USER()	USER()	@@session.proxy_user
proxy_none@localhost	proxy_none@localhost	NULL
SHOW GRANTS;
Grants for proxy_none@localhost
GRANT USAGE ON *.* TO `proxy_none`@`localhost`
GRANT PROXY ON 'proxy_blah'@'localhost' TO 'proxy_none'@'localhost'
** Connecting as proxy_sha256_1 with proxy mapping disabled (sha256 mapping on)**
SELECT CURRENT_USER(), USER(), @@session.proxy_user;
CURRENT_USER()	USER()	@@session.proxy_user
proxy_sha256_1@localhost	proxy_sha256_1@localhost	NULL
SHOW GRANTS;
Grants for proxy_sha256_1@localhost
GRANT DELETE ON *.* TO `proxy_sha256_1`@`localhost`
GRANT PROXY ON 'proxy_base_multi'@'localhost' TO 'proxy_sha256_1'@'localhost'
** Connecting as proxy_sha256_2 with proxy mapping disabled (sha256 mapping on) **
SELECT CURRENT_USER(), USER(), @@session.proxy_user;
CURRENT_USER()	USER()	@@session.proxy_user
proxy_sha256_2@localhost	proxy_sha256_2@localhost	NULL
SHOW GRANTS;
Grants for proxy_sha256_2@localhost
GRANT INSERT, DELETE ON *.* TO `proxy_sha256_2`@`localhost`
GRANT PROXY ON 'proxy_base_multi'@'localhost' TO 'proxy_sha256_2'@'localhost'
** Connecting as proxy_sha256_4 with proxy mapping disabled (sha256 mapping on) **
SELECT CURRENT_USER(), USER(), @@session.proxy_user;
CURRENT_USER()	USER()	@@session.proxy_user
proxy_sha256_4@localhost	proxy_sha256_4@localhost	NULL
SHOW GRANTS;
Grants for proxy_sha256_4@localhost
GRANT INSERT ON *.* TO `proxy_sha256_4`@`localhost`
GRANT PROXY ON 'proxy_base'@'localhost' TO 'proxy_sha256_4'@'localhost'
GRANT PROXY ON 'proxy_base_multi'@'localhost' TO 'proxy_sha256_4'@'localhost'
** Connecting as chained proxy_sha256_41 with proxy mapping disabled (sha256 mapping on) **
SELECT CURRENT_USER(), USER(), @@session.proxy_user;
CURRENT_USER()	USER()	@@session.proxy_user
proxy_sha256_41@localhost	proxy_sha256_41@localhost	NULL
SHOW GRANTS;
Grants for proxy_sha256_41@localhost
GRANT UPDATE ON *.* TO `proxy_sha256_41`@`localhost`
GRANT PROXY ON 'proxy_sha256_4'@'localhost' TO 'proxy_sha256_41'@'localhost'
** Connection default **
GRANT PROXY ON ''@localhost TO proxy_sha256_1@localhost;
** Connecting as Anonymus with proxy mapping disabled (sha256 mapping on) **
SELECT @@session.proxy_user IS NOT NULL;
@@session.proxy_user IS NOT NULL
0
** Connection default **
REVOKE PROXY ON ''@localhost FROM proxy_sha256_1@localhost;
** Connection default **
** Disconnecting connections **
'#----- 3.2.2.1 check and sha256 ON --------------------------#'
'#----- 4.2.check_proxy_users=ON: sha256_password_proxy_users=ON ------#';
SET Global check_proxy_users=ON;
** Connecting as proxy_sha256 with proxy mapping enabled **
SELECT CURRENT_USER(), USER(), @@session.proxy_user;
CURRENT_USER()	USER()	@@session.proxy_user
proxy_base@localhost	proxy_sha256@localhost	'proxy_sha256'@'localhost'
SHOW GRANTS;
Grants for proxy_base@localhost
GRANT CREATE, ALTER ON *.* TO `proxy_base`@`localhost`
** Connection default **
ALTER USER 'proxy_sha256'@'localhost' IDENTIFIED BY 'testpw';
** Connecting as proxy_sha256, password with proxy mapping enabled **
SELECT CURRENT_USER(), USER(), @@session.proxy_user;
CURRENT_USER()	USER()	@@session.proxy_user
proxy_base@localhost	proxy_sha256@localhost	'proxy_sha256'@'localhost'
SHOW GRANTS;
Grants for proxy_base@localhost
GRANT CREATE, ALTER ON *.* TO `proxy_base`@`localhost`
** Connection default **
ALTER USER 'proxy_sha256'@'localhost' IDENTIFIED BY '';
** Connecting as proxy_sha256 with proxy mapping enabled **
SELECT CURRENT_USER(), USER(), @@session.proxy_user;
CURRENT_USER()	USER()	@@session.proxy_user
proxy_none@localhost	proxy_none@localhost	NULL
SHOW GRANTS;
Grants for proxy_none@localhost
GRANT USAGE ON *.* TO `proxy_none`@`localhost`
GRANT PROXY ON 'proxy_blah'@'localhost' TO 'proxy_none'@'localhost'
** Connecting as proxy_sha256_1 with proxy mapping enabled **
SELECT CURRENT_USER(), USER(), @@session.proxy_user;
CURRENT_USER()	USER()	@@session.proxy_user
proxy_base_multi@localhost	proxy_sha256_1@localhost	'proxy_sha256_1'@'localhost'
SHOW GRANTS;
Grants for proxy_base_multi@localhost
GRANT SELECT ON *.* TO `proxy_base_multi`@`localhost`
** Connecting as proxy_sha256_2 with proxy mapping enabled **
SELECT CURRENT_USER(), USER(), @@session.proxy_user;
CURRENT_USER()	USER()	@@session.proxy_user
proxy_base_multi@localhost	proxy_sha256_2@localhost	'proxy_sha256_2'@'localhost'
SHOW GRANTS;
Grants for proxy_base_multi@localhost
GRANT SELECT ON *.* TO `proxy_base_multi`@`localhost`
** Connecting as proxy_sha256_4 with proxy mapping enabled **
SELECT CURRENT_USER() IN ('proxy_base@localhost','proxy_base_multi@localhost');
CURRENT_USER() IN ('proxy_base@localhost','proxy_base_multi@localhost')
1
SELECT USER(), @@session.proxy_user;
USER()	@@session.proxy_user
proxy_sha256_4@localhost	'proxy_sha256_4'@'localhost'
SHOW GRANTS;
Grants for proxy_base@localhost
GRANT CREATE, ALTER ON *.* TO `proxy_base`@`localhost`
** Connecting as chained proxy_sha256_41 with proxy mapping enabled **
SELECT CURRENT_USER() IN ('proxy_base@localhost','proxy_base_multi@localhost');
CURRENT_USER() IN ('proxy_base@localhost','proxy_base_multi@localhost')
0
SELECT CURRENT_USER() IN ('proxy_sha256_4@localhost');
CURRENT_USER() IN ('proxy_sha256_4@localhost')
1
SELECT USER(), @@session.proxy_user;
USER()	@@session.proxy_user
proxy_sha256_41@localhost	'proxy_sha256_41'@'localhost'
SHOW GRANTS;
Grants for proxy_sha256_4@localhost
GRANT INSERT ON *.* TO `proxy_sha256_4`@`localhost`
GRANT PROXY ON 'proxy_base'@'localhost' TO 'proxy_sha256_4'@'localhost'
GRANT PROXY ON 'proxy_base_multi'@'localhost' TO 'proxy_sha256_4'@'localhost'
** Connection default **
GRANT PROXY ON ''@localhost TO proxy_sha256_1@localhost;
** Connecting as Anonymus with proxy mapping enabled **
SELECT @@session.proxy_user IS NOT NULL;
@@session.proxy_user IS NOT NULL
0
** Connection default **
REVOKE PROXY ON ''@localhost FROM proxy_sha256_1@localhost;
** Disconnecting connections **
** Connection default **
DROP USER ''@localhost;
DROP USER proxy_base@localhost;
DROP USER proxy_base_multi@localhost;
DROP USER proxy_sha256@localhost;
DROP USER proxy_none@localhost;
DROP USER proxy_sha256_1@localhost;
DROP USER proxy_sha256_2@localhost;
DROP USER proxy_sha256_3@localhost;
DROP USER proxy_sha256_4@localhost;
DROP USER proxy_sha256_41@localhost;
DROP USER sameea;
SET @@global.sha256_password_proxy_users = @default_sha256_password_proxy_users;
SET @@global.check_proxy_users = @default_check_proxy_users;
