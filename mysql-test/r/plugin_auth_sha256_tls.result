SHOW STATUS LIKE 'Ssl_cipher';
Variable_name	Value
Ssl_cipher	SSL_CIPHER
CREATE USER 'kristofer' IDENTIFIED WITH 'sha256_password';
ALTER USER 'kristofer' IDENTIFIED BY 'secret';
DROP USER 'kristofer';
CREATE USER 'kristofer'@'localhost' IDENTIFIED WITH 'sha256_password' BY '123';
ALTER USER 'kristofer'@'localhost' IDENTIFIED BY 'secret2';
SELECT USER(),CURRENT_USER();
USER()	CURRENT_USER()
kristofer@localhost	kristofer@localhost
SHOW GRANTS FOR 'kristofer'@'localhost';
Grants for kristofer@localhost
GRANT USAGE ON *.* TO `kristofer`@`localhost`
DROP USER 'kristofer'@'localhost';
CREATE USER 'kristofer'@'localhost' IDENTIFIED WITH 'sha256_password' BY '123';
ALTER USER 'kristofer'@'localhost' IDENTIFIED BY '';
SELECT USER(),CURRENT_USER();
USER()	CURRENT_USER()
kristofer@localhost	kristofer@localhost
SHOW GRANTS FOR 'kristofer'@'localhost';
Grants for kristofer@localhost
GRANT USAGE ON *.* TO `kristofer`@`localhost`
DROP USER 'kristofer'@'localhost';
CREATE USER 'kristofer'@'33.33.33.33' IDENTIFIED WITH 'sha256_password' BY '123';
ALTER USER 'kristofer'@'33.33.33.33' IDENTIFIED BY '';
Connection should fail for localhost
ERROR 28000: Access denied for user 'kristofer'@'localhost' (using password: NO)
DROP USER 'kristofer'@'33.33.33.33';
#
# BUG#14807074 ALTER USER PASSWORD EXPIRE VS SHA256_PASSWORD
#
CREATE USER 'u1'@'localhost' IDENTIFIED WITH 'sha256_password';
ALTER USER 'u1'@'localhost' IDENTIFIED BY 'pass';
ALTER USER 'u1'@'localhost' PASSWORD EXPIRE;
SELECT USER();
ERROR HY000: You must reset your password using ALTER USER statement before executing this statement.
ALTER USER 'u1'@'localhost' IDENTIFIED BY 'pass2';
SELECT USER();
USER()
u1@localhost
DROP USER 'u1'@'localhost';
