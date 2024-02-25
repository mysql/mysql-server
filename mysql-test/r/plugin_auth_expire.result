#
# Bug #22551523: ALTER USER IDENTIFIED WITH AUTH_PAM DISABLES USER ACCOUNT
#
CREATE USER b22551523@localhost;
# Must be N
SELECT password_expired from mysql.user where user='b22551523' and host = 'localhost';
password_expired
N
ALTER USER b22551523@localhost IDENTIFIED with 'test_plugin_server';
# Must still be N
SELECT password_expired from mysql.user where user='b22551523' and host = 'localhost';
password_expired
N
ALTER USER b22551523@localhost IDENTIFIED with 'mysql_native_password';
# Must be Y
SELECT password_expired from mysql.user where user='b22551523' and host = 'localhost';
password_expired
Y
DROP USER b22551523@localhost;
