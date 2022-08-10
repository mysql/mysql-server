CREATE USER u1 IDENTIFIED WITH 'mysql_native_password' BY 'foo';
CREATE USER u2 IDENTIFIED WITH 'caching_sha2_password' BY 'foo';
SHOW VARIABLES LIKE 'default_authentication_plugin';
Variable_name	Value
default_authentication_plugin	mysql_native_password
SELECT user,plugin FROM mysql.user WHERE user='u1' or user='u2';
user	plugin
u1	mysql_native_password
u2	caching_sha2_password
SET GLOBAL debug='+d,assert_authentication_roundtrips';
SELECT @@debug,current_user();
@@debug	current_user()
d,assert_authentication_roundtrips	u1@%
ERROR 28000: Access denied for user 'u2'@'localhost' (using password: NO)
SET GLOBAL debug='-d,assert_authentication_roundtrips';
SELECT @@debug,current_user();
@@debug	current_user()
	u2@%
DROP USER u1;
DROP USER u2;
