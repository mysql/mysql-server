SHOW VARIABLES LIKE 'disconnect_on_expired_password';
Variable_name	Value
disconnect_on_expired_password	OFF
## Test mysql client in non-interactive mode
CREATE USER 'bernt';
ALTER USER 'bernt' IDENTIFIED BY 'secret';
ALTER USER 'bernt' PASSWORD EXPIRE;
# Should connect, but doing something should fail
mysql: [Warning] Using a password on the command line interface can be insecure.
ERROR 1820 (HY000) at line 1: You must reset your password using ALTER USER statement before executing this statement.
# Login and set password should succeed
mysql: [Warning] Using a password on the command line interface can be insecure.
DROP USER 'bernt';
## Test mysqltest login
CREATE USER 'bernt';
ALTER USER 'bernt' IDENTIFIED BY 'secret';
ALTER USER 'bernt' PASSWORD EXPIRE;
# Login with mysqltest should work
# But doing something should fail
SELECT 1;
ERROR HY000: You must reset your password using ALTER USER statement before executing this statement.
DROP USER 'bernt';
## Test mysqladmin
CREATE USER 'bernt';
ALTER USER 'bernt' IDENTIFIED BY 'secret';
GRANT ALL ON *.* TO 'bernt' WITH GRANT OPTION;
ALTER USER 'bernt' PASSWORD EXPIRE;
# Doing something should connect but fail
mysqladmin: [Warning] Using a password on the command line interface can be insecure.
mysqladmin: reload failed; error: 'You must reset your password using ALTER USER statement before executing this statement.'
# Setting password should succeed
mysqladmin: [Warning] Using a password on the command line interface can be insecure.
DROP USER 'bernt';
