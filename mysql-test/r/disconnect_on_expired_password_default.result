SHOW VARIABLES LIKE 'disconnect_on_expired_password';
Variable_name	Value
disconnect_on_expired_password	ON
## Test mysql client in non-interactrive mode
CREATE USER 'bernt';
ALTER USER 'bernt' IDENTIFIED BY 'secret';
ALTER USER 'bernt' PASSWORD EXPIRE;
# Attempt to login should fail
mysql: [Warning] Using a password on the command line interface can be insecure.
Please use --connect-expired-password option or invoke mysql in interactive mode.
DROP USER 'bernt';
## Test mysqltest
CREATE USER 'bernt';
ALTER USER 'bernt' IDENTIFIED BY 'secret';
ALTER USER 'bernt' PASSWORD EXPIRE;
# Login with mysqltest should work
# But doing something should fail
SELECT 1;
ERROR HY000: You must reset your password using ALTER USER statement before executing this statement.
# Setting password should work
ALTER USER 'bernt' IDENTIFIED BY 'secret';
DROP USER 'bernt';
## Test mysqladmin
CREATE USER 'bernt';
ALTER USER 'bernt' IDENTIFIED BY 'secret';
GRANT ALL ON *.* TO 'bernt' WITH GRANT OPTION;
ALTER USER 'bernt' PASSWORD EXPIRE;
# Doing something should not connect
mysqladmin: [Warning] Using a password on the command line interface can be insecure.
mysqladmin: connect to server at 'localhost' failed
error: 'Your password has expired. To log in you must change it using a client that supports expired passwords.'
# Setting password should succeed
mysqladmin: [Warning] Using a password on the command line interface can be insecure.
DROP USER 'bernt';
