CREATE USER 'user1'@'%' require ssl;
grant all on *.* to 'user1'@'%' ;
# Testing of all clients with ssl mode PREFERRED
mysql.user                                         OK
Database: mysql  Table: user  Wildcard: user
+-------+----------+-----------+------+-----+---------+-------+---------------------------------+---------+
| Field | Type     | Collation | Null | Key | Default | Extra | Privileges                      | Comment |
+-------+----------+-----------+------+-----+---------+-------+---------------------------------+---------+
| User  | char(32) | utf8_bin  | NO   | PRI |         |       | select,insert,update,references |         |
+-------+----------+-----------+------+-----+---------+-------+---------------------------------+---------+
mysql.test: Records: 1  Deleted: 0  Skipped: 0  Warnings: 1
# Testing of all clients with ssl mode REQUIRED
mysql.user                                         OK
Database: mysql  Table: user  Wildcard: user
+-------+----------+-----------+------+-----+---------+-------+---------------------------------+---------+
| Field | Type     | Collation | Null | Key | Default | Extra | Privileges                      | Comment |
+-------+----------+-----------+------+-----+---------+-------+---------------------------------+---------+
| User  | char(32) | utf8_bin  | NO   | PRI |         |       | select,insert,update,references |         |
+-------+----------+-----------+------+-----+---------+-------+---------------------------------+---------+
mysql.test: Records: 1  Deleted: 0  Skipped: 0  Warnings: 1
# Testing of all clients with ssl mode VERIFY_CA
mysql.user                                         OK
Database: mysql  Table: user  Wildcard: user
+-------+----------+-----------+------+-----+---------+-------+---------------------------------+---------+
| Field | Type     | Collation | Null | Key | Default | Extra | Privileges                      | Comment |
+-------+----------+-----------+------+-----+---------+-------+---------------------------------+---------+
| User  | char(32) | utf8_bin  | NO   | PRI |         |       | select,insert,update,references |         |
+-------+----------+-----------+------+-----+---------+-------+---------------------------------+---------+
mysql.test: Records: 1  Deleted: 0  Skipped: 0  Warnings: 1
# Testing of all clients with ssl mode VERIFY_IDENTITY
mysql.user                                         OK
Database: mysql  Table: user  Wildcard: user
+-------+----------+-----------+------+-----+---------+-------+---------------------------------+---------+
| Field | Type     | Collation | Null | Key | Default | Extra | Privileges                      | Comment |
+-------+----------+-----------+------+-----+---------+-------+---------------------------------+---------+
| User  | char(32) | utf8_bin  | NO   | PRI |         |       | select,insert,update,references |         |
+-------+----------+-----------+------+-----+---------+-------+---------------------------------+---------+
mysql.test: Records: 1  Deleted: 0  Skipped: 0  Warnings: 1
# Tests related to deprecation of ssl options
# Tests related to invalid option of --ssl-mode
# Tests related to valid values of --ssl-mode
Variable_name	Value
Ssl_cipher	
Variable_name	Value
Ssl_cipher	DHE-RSA-AES256-SHA
Variable_name	Value
Ssl_cipher	DHE-RSA-AES256-SHA
Variable_name	Value
Ssl_cipher	DHE-RSA-AES256-SHA
Variable_name	Value
Ssl_cipher	DHE-RSA-AES256-SHA
# Check behavior when --ssl-mode is not passed explicitly
Variable_name	Value
Ssl_cipher	DHE-RSA-AES256-SHA
# Check behavior when multiple times --ssl-mode is passed
Variable_name	Value
Ssl_cipher	DHE-RSA-AES256-SHA
# Check the legacy options
# Cleanup
DROP USER 'user1'@'%';
DROP TABLE mysql.test;
