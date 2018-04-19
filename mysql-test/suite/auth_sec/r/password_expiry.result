######################################################
Authentication cases
######################################################
#### Honouring the value of password_lifetime, below test checks if 
#### password expiry is working when expiry date passed. Use SET PASSWORD
#### for an expired user which honours the value of password_lifetime
CREATE USER u1 IDENTIFIED by 'u1';
UPDATE mysql.user SET password_last_changed=DATE_SUB(NOW(), INTERVAL 3 DAY), 
password_lifetime=2 WHERE user='u1';
FLUSH PRIVILEGES;
#### Connection with u1 will succeed in sand box mode, but no statements
#### can be executed except SET PASSWORD
# Setting variables should work
SELECT 'Password_Expired_SandBoxMode_Test';
ERROR HY000: You must reset your password using ALTER USER statement before executing this statement.
SET PASSWORD = 'u1_new';
SELECT 'Normal_Statement_Can_Be_Executed';
Normal_Statement_Can_Be_Executed
Normal_Statement_Can_Be_Executed
SELECT password_expired FROM mysql.user WHERE user='u1';
password_expired
N
DROP USER u1;
#### Honouring the value of password_lifetime, check if password expiry
#### is working when password is active.	
CREATE USER u1 IDENTIFIED by 'u1';
UPDATE mysql.user SET password_last_changed=DATE_SUB(NOW(), INTERVAL 2 DAY), 
password_lifetime=3 WHERE user='u1';
FLUSH PRIVILEGES;
#### Connection will succeed, user should be able to execute statements
# Setting variables should work
SELECT 'Normal_Statement_Can_Be_Executed';
Normal_Statement_Can_Be_Executed
Normal_Statement_Can_Be_Executed
SELECT password_expired FROM mysql.user WHERE user='u1';
password_expired
N
DROP USER u1;
#### Set the value of password_lifetime to 0 and check its validity.
CREATE USER u1 IDENTIFIED by 'u1';
UPDATE mysql.user SET password_last_changed=DATE_SUB(NOW(), INTERVAL 2 DAY), 
password_lifetime=0 WHERE user='u1';
FLUSH PRIVILEGES;
#### Connection will succeed, user should be able to execute statements
# Setting variables should work
SELECT 'Normal_Statement_Can_Be_Executed';
Normal_Statement_Can_Be_Executed
Normal_Statement_Can_Be_Executed
SELECT password_expired FROM mysql.user WHERE user='u1';
password_expired
N
DROP USER u1;
#### Honouring the value of default_password_lifetime, check if password
#### expiry is working when password is expired.
CREATE USER u1 IDENTIFIED by 'u1';
UPDATE mysql.user SET password_last_changed=DATE_SUB(NOW(), INTERVAL 3 DAY), 
password_lifetime=null WHERE user='u1';
SET GLOBAL default_password_lifetime = 2;
FLUSH PRIVILEGES;
#### Connection will succeed, user should be able to execute statements
SELECT 'Password_Expired_SandBoxMode_Test';
ERROR HY000: You must reset your password using ALTER USER statement before executing this statement.
SELECT password_expired FROM mysql.user WHERE user='u1';
password_expired
N
DROP USER u1;
#### Honouring the value of default_password_lifetime, check if 
#### password expiry is working when password is expired.
CREATE USER u1 IDENTIFIED by 'u1';
UPDATE mysql.user SET password_last_changed=DATE_SUB(NOW(), INTERVAL 2 DAY), 
password_lifetime=null WHERE user='u1';
SET GLOBAL default_password_lifetime = 3;
FLUSH PRIVILEGES;
#### Connection will succeed, user should be able to execute statements
# Setting variables should work
SELECT 'Normal_Statement_Can_Be_Executed';
Normal_Statement_Can_Be_Executed
Normal_Statement_Can_Be_Executed
SELECT password_expired FROM mysql.user WHERE user='u1';
password_expired
N
DROP USER u1;
#### Check if NULL value can be set to default_password_lifetime
SET GLOBAL default_password_lifetime = null;
ERROR 42000: Incorrect argument type to variable 'default_password_lifetime'
#### Ensure that existing sessions are not disturbed due to change in 
#### global value of default_password_lifetime.
CREATE USER u1 IDENTIFIED by 'u1';
#### Connection will succeed, user should be able to execute statements
# Setting variables should work
SELECT 'Normal_Statement_Can_Be_Executed';
Normal_Statement_Can_Be_Executed
Normal_Statement_Can_Be_Executed
UPDATE mysql.user SET password_last_changed=DATE_SUB(NOW(), INTERVAL 3 DAY), 
password_lifetime=null WHERE user='u1';
SET GLOBAL default_password_lifetime = 2;
FLUSH PRIVILEGES;
# Setting variables should work
SELECT 'Normal_Statement_Can_Be_Executed';
Normal_Statement_Can_Be_Executed
Normal_Statement_Can_Be_Executed
SELECT password_expired FROM mysql.user WHERE user='u1';
password_expired
N
DROP USER u1;
#### Use SET PASSWORD for an expired user which honours the value of 
#### variable default_password_lifetime.
CREATE USER u1 IDENTIFIED by 'u1';
UPDATE mysql.user SET password_last_changed=DATE_SUB(NOW(), INTERVAL 3 DAY), 
password_lifetime=null WHERE user='u1';
SET GLOBAL default_password_lifetime = 2;
FLUSH PRIVILEGES;
#### Connection will succeed in sand box mode, 
#### but no statements can be executed except SET PASSWORD
SELECT 'Password_Expired_SandBoxMode_Test';
ERROR HY000: You must reset your password using ALTER USER statement before executing this statement.
SET PASSWORD = 'u1_new';
# Setting variables should work
SELECT 'Normal_Statement_Can_Be_Executed';
Normal_Statement_Can_Be_Executed
Normal_Statement_Can_Be_Executed
SELECT password_expired FROM mysql.user WHERE user='u1';
password_expired
N
DROP USER u1;
#### For an expired user where password_expired is Y, ensure that it 
#### does not execute anything except SET PASSWORD.
CREATE USER u1 IDENTIFIED by 'u1';
UPDATE mysql.user SET password_expired='Y' WHERE user='u1';
FLUSH PRIVILEGES;
#Below statement should not affect the value of password_expired column
ALTER USER 'u1' PASSWORD EXPIRE INTERVAL 5 DAY;
SELECT password_expired FROM mysql.user WHERE user='u1';
password_expired
Y
DROP USER u1;
#### Check if SET PASSWORD resets the value of column
####  password_expired to N when it is Y.
CREATE USER u1 IDENTIFIED by 'u1';
ALTER USER 'u1' PASSWORD EXPIRE;
#Expiry status of u1 should be Y
SELECT password_expired FROM mysql.user WHERE user='u1';
password_expired
Y
SET PASSWORD = 'u1_new';
#Expiry status of u1 should be N
SELECT password_expired FROM mysql.user WHERE user='u1';
password_expired
N
DROP USER u1;
#### Try updating the newly introduced columns and try setting the 
#### global variable default_password_lifetime with an user having
####  insufficient privilege.
CREATE USER u1 IDENTIFIED by 'u1';
SET GLOBAL default_password_lifetime = 2;
ERROR 42000: Access denied; you need (at least one of) the SUPER or SYSTEM_VARIABLES_ADMIN privilege(s) for this operation
UPDATE mysql.user SET password_last_changed=DATE_SUB(NOW(), INTERVAL 3 DAY), 
password_lifetime=null WHERE user='u1';
ERROR 42000: UPDATE command denied to user 'u1'@'localhost' for table 'user'
DROP USER u1;
#### One year password expiration check
CREATE USER u1 IDENTIFIED by 'u1';
UPDATE mysql.user SET password_last_changed=DATE_SUB(NOW(), INTERVAL 361 DAY), 
password_lifetime=null WHERE user='u1';
SET GLOBAL default_password_lifetime = 360;
FLUSH PRIVILEGES;
# Connection will succeed in sand box mode, 
# but no statements can be executed except SET PASSWORD
SELECT 'Password_Expired_SandBoxMode_Test';
ERROR HY000: You must reset your password using ALTER USER statement before executing this statement.
SET PASSWORD = 'u1_new';
# Setting variables should work
SELECT 'Normal_Statement_Can_Be_Executed';
Normal_Statement_Can_Be_Executed
Normal_Statement_Can_Be_Executed
SELECT password_expired FROM mysql.user WHERE user='u1';
password_expired
N
DROP USER u1;
#### Cleanup statements
SET GLOBAL default_password_lifetime = default;
