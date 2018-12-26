CREATE USER 'kristofer' IDENTIFIED BY 'secret';
SELECT user FROM mysql.user ORDER BY user;
user
kristofer
mysql.infoschema
mysql.session
mysql.sys
root
user()	current_user()
kristofer@localhost	kristofer@%
user()	current_user()
kristofer@localhost	kristofer@%
user()	current_user()
kristofer@localhost	kristofer@%
DROP USER 'kristofer';
CREATE USER 'kristofer'@'localhost' IDENTIFIED BY 'secret2';
user()	current_user()
kristofer@localhost	kristofer@localhost
user()	current_user()
kristofer@localhost	kristofer@localhost
user()	current_user()
kristofer@localhost	kristofer@localhost
SHOW GRANTS FOR 'kristofer'@'localhost';
Grants for kristofer@localhost
GRANT USAGE ON *.* TO `kristofer`@`localhost`
DROP USER 'kristofer'@'localhost';
CREATE USER 'kristofer'@'localhost' IDENTIFIED BY '123';
user()	current_user()
kristofer@localhost	kristofer@localhost
user()	current_user()
kristofer@localhost	kristofer@localhost
user()	current_user()
kristofer@localhost	kristofer@localhost
SHOW GRANTS FOR 'kristofer'@'localhost';
Grants for kristofer@localhost
GRANT USAGE ON *.* TO `kristofer`@`localhost`
DROP USER 'kristofer'@'localhost';
CREATE USER 'kristofer'@'33.33.33.33' IDENTIFIED BY '123';
Connection should fail for localhost
ERROR 28000: Access denied for user 'kristofer'@'localhost' (using password: NO)
DROP USER 'kristofer'@'33.33.33.33';
