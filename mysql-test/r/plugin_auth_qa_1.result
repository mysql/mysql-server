CALL mtr.add_suppression("Plugin test_plugin_server reported: 'Wrong password supplied for plug_dest'");
CALL mtr.add_suppression("The plugin 'new_plugin_server' used to authenticate user 'new_user'@'%' is not loaded. Nobody can currently login using this account.");
CREATE DATABASE test_user_db;
SELECT user,plugin FROM mysql.user WHERE user NOT IN ('root', 'mysql.sys', 'mysql.session', 'mysql.infoschema');
user	plugin
========== test 1.2 ========================================
CREATE USER plug_user
IDENTIFIED WITH test_plugin_server AS 'plug_dest';
CREATE USER plug_dest IDENTIFIED BY 'plug_dest_passwd';
mysql: [Warning] Using a password on the command line interface can be insecure.
ERROR 1045 (28000): Access denied for user 'plug_user'@'localhost' (using password: YES)
GRANT PROXY ON plug_dest TO plug_user;
mysql: [Warning] Using a password on the command line interface can be insecure.
current_user()
plug_dest@%
user()
plug_user@localhost
RENAME USER plug_dest TO new_dest;
mysql: [Warning] Using a password on the command line interface can be insecure.
ERROR 1045 (28000): Access denied for user 'plug_user'@'localhost' (using password: YES)
GRANT PROXY ON new_dest TO plug_user;
mysql: [Warning] Using a password on the command line interface can be insecure.
ERROR 1045 (28000): Access denied for user 'plug_user'@'localhost' (using password: YES)
SELECT user,plugin FROM mysql.user WHERE user NOT IN ('root', 'mysql.sys', 'mysql.session', 'mysql.infoschema');
user	plugin
new_dest	caching_sha2_password
plug_user	test_plugin_server
DROP USER plug_user,new_dest;
CREATE USER plug_user
IDENTIFIED WITH test_plugin_server AS 'plug_dest';
CREATE USER plug_dest IDENTIFIED BY 'plug_dest_passwd';
GRANT PROXY ON plug_dest TO plug_user;
connect(plug_user,localhost,plug_user,plug_dest);
select USER(),CURRENT_USER();
USER()	CURRENT_USER()
plug_user@localhost	plug_dest@%
connection default;
disconnect plug_user;
RENAME USER plug_user TO new_user;
connect(plug_user,localhost,new_user,plug_dest);
select USER(),CURRENT_USER();
USER()	CURRENT_USER()
new_user@localhost	plug_dest@%
connection default;
SELECT user,plugin FROM mysql.user WHERE user NOT IN ('root', 'mysql.sys', 'mysql.session', 'mysql.infoschema');
user	plugin
new_user	test_plugin_server
plug_dest	caching_sha2_password
disconnect plug_user;
UPDATE mysql.user SET user='plug_user' WHERE user='new_user';
FLUSH PRIVILEGES;
SELECT user,plugin FROM mysql.user WHERE user NOT IN ('root', 'mysql.sys', 'mysql.session', 'mysql.infoschema');
user	plugin
plug_dest	caching_sha2_password
plug_user	test_plugin_server
DROP USER plug_dest,plug_user;
========== test 1.3 ========================================
CREATE USER plug_user
IDENTIFIED WITH test_plugin_server AS 'plug_dest';
CREATE USER plug_dest IDENTIFIED BY 'plug_dest_passwd';
GRANT PROXY ON plug_dest TO plug_user;
connect(plug_user,localhost,plug_user,plug_dest);
select USER(),CURRENT_USER();
USER()	CURRENT_USER()
plug_user@localhost	plug_dest@%
connection default;
disconnect plug_user;
SELECT user,plugin FROM mysql.user WHERE user NOT IN ('root', 'mysql.sys', 'mysql.session', 'mysql.infoschema');
user	plugin
plug_dest	caching_sha2_password
plug_user	test_plugin_server
UPDATE mysql.user SET user='new_user' WHERE user='plug_user';
FLUSH PRIVILEGES;
SELECT user,plugin FROM mysql.user WHERE user NOT IN ('root', 'mysql.sys', 'mysql.session', 'mysql.infoschema');
user	plugin
new_user	test_plugin_server
plug_dest	caching_sha2_password
UPDATE mysql.user SET authentication_string='new_dest' WHERE user='new_user';
FLUSH PRIVILEGES;
SELECT user,plugin FROM mysql.user WHERE user NOT IN ('root', 'mysql.sys', 'mysql.session', 'mysql.infoschema');
user	plugin
new_user	test_plugin_server
plug_dest	caching_sha2_password
UPDATE mysql.user SET plugin='new_plugin_server' WHERE user='new_user';
FLUSH PRIVILEGES;
SELECT user,plugin FROM mysql.user WHERE user NOT IN ('root', 'mysql.sys', 'mysql.session', 'mysql.infoschema');
user	plugin
new_user	new_plugin_server
plug_dest	caching_sha2_password
connect(plug_user,localhost,new_user,new_dest);
ERROR HY000: Plugin 'new_plugin_server' is not loaded
UPDATE mysql.user SET plugin='test_plugin_server' WHERE user='new_user';
UPDATE mysql.user SET USER='new_dest' WHERE user='plug_dest';
FLUSH PRIVILEGES;
GRANT PROXY ON new_dest TO new_user;
SELECT user,plugin FROM mysql.user WHERE user NOT IN ('root', 'mysql.sys', 'mysql.session', 'mysql.infoschema');
user	plugin
new_dest	caching_sha2_password
new_user	test_plugin_server
connect(plug_user,localhost,new_user,new_dest);
select USER(),CURRENT_USER();
USER()	CURRENT_USER()
new_user@localhost	new_dest@%
connection default;
disconnect plug_user;
UPDATE mysql.user SET USER='plug_dest' WHERE user='new_dest';
FLUSH PRIVILEGES;
CREATE USER new_dest IDENTIFIED BY 'new_dest_passwd';
SELECT user,plugin FROM mysql.user WHERE user NOT IN ('root', 'mysql.sys', 'mysql.session', 'mysql.infoschema');
user	plugin
new_dest	caching_sha2_password
new_user	test_plugin_server
plug_dest	caching_sha2_password
GRANT ALL PRIVILEGES ON test.* TO new_user;
connect(plug_user,localhost,new_dest,new_dest_passwd);
select USER(),CURRENT_USER();
USER()	CURRENT_USER()
new_dest@localhost	new_dest@%
connection default;
disconnect plug_user;
DROP USER plug_user, new_user,new_dest,plug_dest;
========== test 2, 2.1, 2.2 ================================
CREATE USER ''@'' IDENTIFIED WITH test_plugin_server AS 'proxied_user';
CREATE USER proxied_user IDENTIFIED BY 'proxied_user_passwd';
SELECT user,plugin FROM mysql.user WHERE user NOT IN ('root', 'mysql.sys', 'mysql.session', 'mysql.infoschema');
user	plugin
	test_plugin_server
proxied_user	caching_sha2_password
connect(proxy_con,localhost,proxied_user,proxied_user_passwd);
SELECT USER(),CURRENT_USER();
USER()	CURRENT_USER()
proxied_user@localhost	proxied_user@%
========== test 2.2.1 ======================================
SELECT @@proxy_user;
@@proxy_user
NULL
connection default;
disconnect proxy_con;
connect(proxy_con,localhost,proxy_user,proxied_user);
ERROR 28000: Access denied for user 'proxy_user'@'localhost' (using password: YES)
GRANT PROXY ON proxied_user TO ''@'';
connect(proxy_con,localhost,proxied_user,proxied_user_passwd);
SELECT USER(),CURRENT_USER();
USER()	CURRENT_USER()
proxied_user@localhost	proxied_user@%
connection default;
disconnect proxy_con;
connect(proxy_con,localhost,proxy_user,proxied_user);
SELECT USER(),CURRENT_USER();
USER()	CURRENT_USER()
proxy_user@localhost	proxied_user@%
========== test 2.2.1 ======================================
SELECT @@proxy_user;
@@proxy_user
''@''
connection default;
disconnect proxy_con;
DROP USER ''@'',proxied_user;
CREATE USER ''@'' IDENTIFIED WITH test_plugin_server AS 'proxied_user';
GRANT ALL PRIVILEGES ON test_user_db.* TO ''@'';
CREATE USER proxied_user IDENTIFIED BY 'proxied_user_passwd';
SELECT user,plugin FROM mysql.user WHERE user NOT IN ('root', 'mysql.sys', 'mysql.session', 'mysql.infoschema');
user	plugin
	test_plugin_server
proxied_user	caching_sha2_password
connect(proxy_con,localhost,proxied_user,proxied_user_passwd);
SELECT USER(),CURRENT_USER();
USER()	CURRENT_USER()
proxied_user@localhost	proxied_user@%
SELECT @@proxy_user;
@@proxy_user
NULL
connection default;
disconnect proxy_con;
connect(proxy_con,localhost,proxy_user,proxied_user);
ERROR 28000: Access denied for user 'proxy_user'@'localhost' (using password: YES)
GRANT PROXY ON proxied_user TO ''@'';
connect(proxy_con,localhost,proxied_user,proxied_user_passwd);
SELECT USER(),CURRENT_USER();
USER()	CURRENT_USER()
proxied_user@localhost	proxied_user@%
connection default;
disconnect proxy_con;
connect(proxy_con,localhost,proxy_user,proxied_user);
SELECT USER(),CURRENT_USER();
USER()	CURRENT_USER()
proxy_user@localhost	proxied_user@%
SELECT @@proxy_user;
@@proxy_user
''@''
connection default;
disconnect proxy_con;
DROP USER ''@'',proxied_user;
CREATE USER ''@'' IDENTIFIED WITH test_plugin_server AS 'proxied_user';
CREATE USER proxied_user_1 IDENTIFIED BY 'proxied_user_1_pwd';
CREATE USER proxied_user_2 IDENTIFIED BY 'proxied_user_2_pwd';
CREATE USER proxied_user_3 IDENTIFIED BY 'proxied_user_3_pwd';
CREATE USER proxied_user_4 IDENTIFIED BY 'proxied_user_4_pwd';
CREATE USER proxied_user_5 IDENTIFIED BY 'proxied_user_5_pwd';
GRANT PROXY ON proxied_user_1 TO ''@'';
GRANT PROXY ON proxied_user_2 TO ''@'';
GRANT PROXY ON proxied_user_3 TO ''@'';
GRANT PROXY ON proxied_user_4 TO ''@'';
GRANT PROXY ON proxied_user_5 TO ''@'';
SELECT user,plugin FROM mysql.user WHERE user NOT IN ('root', 'mysql.sys', 'mysql.session', 'mysql.infoschema');
user	plugin
	test_plugin_server
proxied_user_1	caching_sha2_password
proxied_user_2	caching_sha2_password
proxied_user_3	caching_sha2_password
proxied_user_4	caching_sha2_password
proxied_user_5	caching_sha2_password
connect(proxy_con_1,localhost,proxied_user_1,'proxied_user_1_pwd');
connect(proxy_con_2,localhost,proxied_user_2,proxied_user_2_pwd);
connect(proxy_con_3,localhost,proxied_user_3,proxied_user_3_pwd);
connect(proxy_con_4,localhost,proxied_user_4,proxied_user_4_pwd);
connect(proxy_con_5,localhost,proxied_user_5,proxied_user_5_pwd);
connection proxy_con_1;
SELECT USER(),CURRENT_USER();
USER()	CURRENT_USER()
proxied_user_1@localhost	proxied_user_1@%
SELECT @@proxy_user;
@@proxy_user
NULL
connection proxy_con_2;
SELECT USER(),CURRENT_USER();
USER()	CURRENT_USER()
proxied_user_2@localhost	proxied_user_2@%
SELECT @@proxy_user;
@@proxy_user
NULL
connection proxy_con_3;
SELECT USER(),CURRENT_USER();
USER()	CURRENT_USER()
proxied_user_3@localhost	proxied_user_3@%
SELECT @@proxy_user;
@@proxy_user
NULL
connection proxy_con_4;
SELECT USER(),CURRENT_USER();
USER()	CURRENT_USER()
proxied_user_4@localhost	proxied_user_4@%
SELECT @@proxy_user;
@@proxy_user
NULL
connection proxy_con_5;
SELECT USER(),CURRENT_USER();
USER()	CURRENT_USER()
proxied_user_5@localhost	proxied_user_5@%
SELECT @@proxy_user;
@@proxy_user
NULL
connection default;
disconnect proxy_con_1;
disconnect proxy_con_2;
disconnect proxy_con_3;
disconnect proxy_con_4;
disconnect proxy_con_5;
DROP USER ''@'',proxied_user_1,proxied_user_2,proxied_user_3,proxied_user_4,proxied_user_5;
DROP DATABASE test_user_db;
