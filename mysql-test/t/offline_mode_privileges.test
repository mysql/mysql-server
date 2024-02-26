--echo ##
--echo ## WL#13400: Make sure setting offline_mode requires CONNECTION_ADMIN
--echo ##
--echo ## For different clients, test that OFFLINE_MODE can only be modify by those
--echo ## who have CONNECTION_ADMIN plus SYSTEM_VARIABLES_ADMIN or SUPER privileges.
--echo ## Of the following, only those with + prefix should be able to modify OFFLINE_MODE:
--echo ## - base_session:     having no privileges
--echo ## - regular_session:  having SYSTEM_VARIABLES_ADMIN privilege
--echo ## - power_session:    having SYSTEM_VARIABLES_ADMIN and SYSTEM_USER privileges
--echo ## + super_session:    having SUPER privileges
--echo ## + admin_session:    having SYSTEM_VARIABLES_ADMIN and CONNECTION_ADMIN privileges
--echo ## + superadmin_session: having SYSTEM_VARIABLES_ADMIN and SUPER and CONNECTION_ADMIN privileges
--echo ## + sysadmin_session: having SYSTEM_VARIABLES_ADMIN, CONNECTION_ADMIN, SYSTEM_USER privileges
--echo ##

SET @original_offline_mode = @@global.offline_mode;

--echo
--echo ### Setup ###

--echo
--echo # Create base_session user
CREATE USER base@localhost IDENTIFIED BY 'base';

--echo
--echo # Create regular_session user
CREATE USER regular@localhost IDENTIFIED BY 'regular';
GRANT SYSTEM_VARIABLES_ADMIN ON *.* TO regular@localhost;

--echo
--echo # Create power_session user
CREATE USER power@localhost IDENTIFIED BY 'power';
GRANT SYSTEM_USER, SYSTEM_VARIABLES_ADMIN ON *.* TO power@localhost;

--echo
--echo # Create super_session user
CREATE USER super@localhost IDENTIFIED BY 'super';
GRANT SUPER ON *.* TO super@localhost;

--echo
--echo # Create admin_session user
CREATE USER admin@localhost IDENTIFIED BY 'admin';
GRANT SYSTEM_VARIABLES_ADMIN, CONNECTION_ADMIN ON *.* TO admin@localhost;

--echo
--echo # Create superadmin_session user
CREATE USER superadmin@localhost IDENTIFIED BY 'superadmin';
GRANT SYSTEM_VARIABLES_ADMIN, CONNECTION_ADMIN, SUPER ON *.* TO superadmin@localhost;

--echo
--echo # Create sysadmin_session user (max privileges)
CREATE USER sysadmin@localhost IDENTIFIED BY 'sysadmin';
GRANT SYSTEM_VARIABLES_ADMIN, CONNECTION_ADMIN, SYSTEM_USER ON *.* TO sysadmin@localhost;

--echo
--echo ## TEST 1: base_session can not modify OFFLINE_MODE
--echo

connect (con_base, localhost, base, base, );
--error ER_SPECIFIC_ACCESS_DENIED_ERROR
SET GLOBAL OFFLINE_MODE=ON;
disconnect con_base;

--echo
--echo ## TEST 2: regular_session can not modify OFFLINE_MODE
--echo

connect (con_regular, localhost, regular, regular, );
--error ER_SPECIFIC_ACCESS_DENIED_ERROR
SET GLOBAL OFFLINE_MODE=ON;
disconnect con_regular;

--echo
--echo ## TEST 3: power_session can not modify OFFLINE_MODE
--echo

connect (con_power, localhost, power, power, );
--error ER_SPECIFIC_ACCESS_DENIED_ERROR
SET GLOBAL OFFLINE_MODE=ON;
disconnect con_power;

--echo
--echo ## TEST 4: super_session can modify OFFLINE_MODE
--echo

connect (con_super, localhost, super, super, );
SET GLOBAL OFFLINE_MODE=ON;
disconnect con_super;

--echo
--echo ## TEST 5: admin_session can modify OFFLINE_MODE
--echo

connect (con_admin, localhost, admin, admin, );
SET GLOBAL OFFLINE_MODE=OFF;
disconnect con_admin;

--echo
--echo ## TEST 6: superadmin_session can modify OFFLINE_MODE
--echo

connect (con_superadmin, localhost, superadmin, superadmin, );
SET GLOBAL OFFLINE_MODE=ON;
disconnect con_superadmin;

--echo
--echo ## TEST 6: sysadmin_session can modify OFFLINE_MODE
--echo

connect (con_sysadmin, localhost, sysadmin, sysadmin, );
SET GLOBAL OFFLINE_MODE=OFF;
disconnect con_sysadmin;

## Cleanup
--echo
--echo # CLEAN UP
--echo
connection default;
DROP USER base@localhost;
DROP USER regular@localhost;
DROP USER power@localhost;
DROP USER super@localhost;
DROP USER admin@localhost;
DROP USER superadmin@localhost;
DROP USER sysadmin@localhost;
SET @@global.offline_mode = @original_offline_mode;
