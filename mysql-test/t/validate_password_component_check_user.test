--source include/have_validate_password_component.inc

# Save the initial number of concurrent sessions
--source include/count_sessions.inc

INSTALL COMPONENT "file://component_validate_password";

# check the default for validate_password.check_user_name
SELECT @@global.validate_password.check_user_name;

--echo # Should fail: not a session variable
--error ER_GLOBAL_VARIABLE
SET @@session.validate_password.check_user_name= ON;

--echo # Should fail: not a session variable
--error ER_GLOBAL_VARIABLE
SET validate_password.check_user_name= ON;

# turn other policies off so they don't stand in the way
SET @@global.validate_password.policy=LOW;
SET @@global.validate_password.mixed_case_count=0;
SET @@global.validate_password.number_count=0;
SET @@global.validate_password.special_char_count=0;
SET @@global.validate_password.length=0;


# check_user_name=ON tests. No need to check with off since it's covered.
SET @@global.validate_password.check_user_name= ON;

--echo # Must pass: password the same as the the user name, but run by root
CREATE USER "base_user"@localhost IDENTIFIED BY 'base_user';
GRANT ALL PRIVILEGES ON test.* TO "base_user"@localhost;
--echo # Must pass: password the same as the user name, but run by root
SET PASSWORD FOR "base_user"@localhost = 'base_user';
--echo # Must pass: password the same as the user name, but run by root
ALTER USER "base_user"@localhost IDENTIFIED BY 'base_user';
--echo # Must fail: password is root
--error ER_NOT_VALID_PASSWORD
CREATE USER foo@localhost IDENTIFIED BY 'root';
--echo # Must return 0 : same as the user name
SELECT VALIDATE_PASSWORD_STRENGTH('root') = 0;
--echo # Must return 0 : same as the user name
SELECT VALIDATE_PASSWORD_STRENGTH('toor') = 0;
--echo # Must return non-0: upper case in user name
SELECT VALIDATE_PASSWORD_STRENGTH('Root') <> 0;
--echo # Must return non-0: upper case in reverse user name
SELECT VALIDATE_PASSWORD_STRENGTH('Toor') <> 0;
--echo # Must return non-0: different name
SELECT VALIDATE_PASSWORD_STRENGTH('fooHoHo%1') <> 0;


--echo # connect as base_user
connect(base_user_con,localhost,base_user,base_user,test);
--echo # Should fail: password the same as the user name
--error ER_NOT_VALID_PASSWORD
SET PASSWORD='base_user';
--echo # Should pass: uppercase in U
SET PASSWORD='base_User';
--echo # Should fail: password the same as the login user name
--error ER_NOT_VALID_PASSWORD
ALTER USER "base_user"@localhost IDENTIFIED BY 'base_user';
--echo # Must be 0: user name matches the password
SELECT VALIDATE_PASSWORD_STRENGTH('base_user') = 0;
--echo # Must be 0: reverse of user name matches the password
SELECT VALIDATE_PASSWORD_STRENGTH('resu_esab') = 0;
--echo # Must pass: empty password is ok
SET PASSWORD='';

--echo # back to default connection
connection default;
disconnect base_user_con;
REVOKE ALL PRIVILEGES ON test.* FROM "base_user"@localhost;
DROP USER "base_user"@localhost;

-- echo # test effective user name
CREATE USER ""@localhost;
GRANT ALL PRIVILEGES ON test.* TO ""@localhost;

--echo # connect as the login_user
connect(login_user_con,localhost,login_user,,test);
SELECT USER(),CURRENT_USER();

--echo # Should return 0: login user id matches
SELECT VALIDATE_PASSWORD_STRENGTH('login_user') = 0;

--echo # Should return 0: reverse login user id matches
SELECT VALIDATE_PASSWORD_STRENGTH('resu_nigol') = 0;

--echo # back to default connection
connection default;
disconnect login_user_con;
REVOKE ALL PRIVILEGES ON test.* FROM ""@localhost;
DROP USER ""@localhost;

SET @@global.validate_password.policy=default;
SET @@global.validate_password.length=default;
SET @@global.validate_password.mixed_case_count=default;
SET @@global.validate_password.number_count=default;
SET @@global.validate_password.special_char_count=default;
SET @@global.validate_password.check_user_name= default;

UNINSTALL COMPONENT "file://component_validate_password";

# Wait till all disconnects are completed
--source include/wait_until_count_sessions.inc
