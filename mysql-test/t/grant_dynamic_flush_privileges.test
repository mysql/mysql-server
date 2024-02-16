--echo #
--echo # WL#16044 : Provide Separate Privilege For FLUSH PRIVILEGES
--echo #

--echo # Show grants for root user, it must have FLUSH_PRIVILEGES.
--source include/show_all_privileges.inc
SHOW GRANTS FOR 'root'@'localhost';

--echo # Create the test user.
CREATE USER test@localhost;

--echo # Switch to test user, make sure FLUSH PRIVILEGES throws access denied.
connect(testcon, localhost, test,,);

--echo # The error mentions about both RELOAD and FLUSH_PRIVILEGES privileges.
--error ER_SPECIFIC_ACCESS_DENIED_ERROR
FLUSH PRIVILEGES;

--echo # Back to root user.
connection default;

--echo # Make sure privilege is not specific to a particular database.
--error ER_ILLEGAL_PRIVILEGE_LEVEL
GRANT FLUSH_PRIVILEGES ON mysql.* TO test@localhost;

--echo # Grant FLUSH_PRIVILEGES and SYSTEM_VARIABLES_ADMIN to the test user.
GRANT FLUSH_PRIVILEGES, SYSTEM_VARIABLES_ADMIN ON *.* TO test@localhost;

--echo # Back to test user.
connection testcon;

--echo # Try to change system variables to test the SYSTEM_VARIABLES_ADMIN privilege.
SET @@GLOBAL.general_log=OFF;
SHOW VARIABLES LIKE 'general_log';
SET @@GLOBAL.general_log=ON;
SHOW VARIABLES LIKE 'general_log';

--echo # Back to root user.
connection default;

--echo # Revoke the SYSTEM_VARIABLES_ADMIN privilege directly from global_grants table.
DELETE FROM mysql.global_grants WHERE user='test' AND priv='SYSTEM_VARIABLES_ADMIN';

--echo # Back to test user.
connection testcon;

--echo # Can still change the system variables.
SET @@GLOBAL.general_log=OFF;
SHOW VARIABLES LIKE 'general_log';
SET @@GLOBAL.general_log=ON;
SHOW VARIABLES LIKE 'general_log';

--echo # Flush the privileges.
FLUSH PRIVILEGES;

--echo # Privileges shall be gone now.
SHOW GRANTS FOR CURRENT_USER;

--echo # Can no longer change system variables.
--error ER_SPECIFIC_ACCESS_DENIED_ERROR
SET @@GLOBAL.general_log=OFF;
SHOW VARIABLES LIKE 'general_log';

--echo # Back to root user.
connection default;

--echo # Create another test user.
CREATE USER test_2@localhost;

--echo # Create a role, and then assign the privilege to the role.
CREATE ROLE flusher;
GRANT FLUSH_PRIVILEGES ON *.* TO flusher;

--echo # Assign the role to the test_2 user.
GRANT flusher TO test_2@localhost;

--echo # Switch to test_2 user.
connect(test_2_con, localhost, test_2, , );

SHOW GRANTS FOR CURRENT_USER;
SHOW GRANTS FOR CURRENT_USER USING flusher;

--echo # Set the role.
SET ROLE flusher;

--echo # test_2 shall be able to execute the FLUSH PRIVILEGES now.
FLUSH PRIVILEGES;
disconnect test_2_con;

--echo # Clean up
connection default;
DROP ROLE flusher;
DROP USER test@localhost;
DROP USER test_2@localhost;
