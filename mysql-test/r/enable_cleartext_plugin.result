#
# Bug #21235226 : THE --ENABLE-CLEARTEXT-PLUGIN IS NOT IMPLEMENTED
#                 IN ALL CLIENT PROGRAMS
#
CREATE DATABASE db21235226;
USE db21235226;
CREATE TABLE t1(a INT);
INSERT INTO t1 VALUES (1), (2);
SELECT * FROM t1;
a
1
2
CREATE USER uplain@localhost IDENTIFIED WITH 'cleartext_plugin_server'
  AS 'cleartext_test';
GRANT ALL PRIVILEGES ON *.* TO uplain@localhost;
mysqldump: [Warning] Using a password on the command line interface can be insecure.
mysqldump: Got error: 2059: Authentication plugin 'mysql_clear_password' cannot be loaded: plugin not enabled when trying to connect
SELECT * FROM t1;
a
mysqlimport: [Warning] Using a password on the command line interface can be insecure.
mysqlimport: Error: 2059 Authentication plugin 'mysql_clear_password' cannot be loaded: plugin not enabled
SELECT * FROM t1;
a
1
2
mysqlshow: [Warning] Using a password on the command line interface can be insecure.
mysqlshow: Authentication plugin 'mysql_clear_password' cannot be loaded: plugin not enabled
Database: db21235226
+--------+
| Tables |
+--------+
| t1     |
+--------+
mysqlcheck: [Warning] Using a password on the command line interface can be insecure.
mysqlcheck: Got error: 2059: Authentication plugin 'mysql_clear_password' cannot be loaded: plugin not enabled when trying to connect
db21235226.t1                                      OK
DROP TABLE t1;
DROP DATABASE db21235226;
DROP USER uplain@localhost;
