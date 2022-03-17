--echo ##
--echo ## WL#14317: Offline_mode: checks for SYSTEM_USER and CONNECTION_ADMIN privileges
--echo ##
--echo ## Define the following client types (only the last two can activate OFFLINE_MODE):
--echo ## - regular_session:  having SYSTEM_VARIABLES_ADMIN privilege
--echo ## - power_session:    having SYSTEM_VARIABLES_ADMIN and SYSTEM_USER privileges
--echo ## - super_session:    having SYSTEM_VARIABLES_ADMIN and CONNECTION_ADMIN privileges
--echo ## - sysadmin_session: having SYSTEM_VARIABLES_ADMIN, CONNECTION_ADMIN, SYSTEM_USER privileges
--echo ## Test that activating OFFLINE_MODE action performed by:
--echo ## 1. super_session kills regular_sessions, but not power_session or super_session or sysadmin_session
--echo ## 2. sysadmin_session kills regular_sessions and power_sessions, but not super_session or sysadmin_session
--echo ##

SET @original_offline_mode = @@global.offline_mode;

--echo
--echo ### Setup ###

--echo
--echo # Create regular_session user
create user regular@localhost identified by 'regular';
grant SYSTEM_VARIABLES_ADMIN on *.* to regular@localhost;

--echo
--echo # Create power_session user
create user power@localhost identified by 'power';
grant SYSTEM_USER, SYSTEM_VARIABLES_ADMIN on *.* to power@localhost;

--echo
--echo # Create super_session user (switches off OFFLINE_MODE)
create user super@localhost identified by 'super';
grant SYSTEM_VARIABLES_ADMIN, CONNECTION_ADMIN on *.* to super@localhost;

--echo
--echo # Create sysadmin_session user (max privileges)
create user sysadmin@localhost identified by 'sysadmin';
grant SYSTEM_VARIABLES_ADMIN, CONNECTION_ADMIN, SYSTEM_USER on *.* to sysadmin@localhost;

--echo
flush privileges;

--echo # Create power_session connection
connect (con_power, localhost, power, power, );

--echo # Create regular_session connection
connect (con_regular, localhost, regular, regular, );

--echo # Create super_session connection (switches off OFFLINE_MODE)
connect (con_super, localhost, super, super, );

--echo # Create sysadmin_session connection (max privileges)
connect (con_sysadmin, localhost, sysadmin, sysadmin, );

--echo
--echo ## TEST 1: super_session activates OFFLINE_MODE
--echo

--echo # Setup - create additional super_session connection
connect (con_super1, localhost, super, super, );

--echo # Activate super_session connection
connection con_super;

--echo # Activate OFFLINE_MODE (with super_session)
SET GLOBAL OFFLINE_MODE=ON;

--echo # Verify that the regular_session has been killed
connection con_regular;
--error CR_SERVER_LOST
SELECT USER();

--echo # Verify that the power_session is kept alive
connection con_power;
SELECT USER();

--echo # Verify that the super_session is kept alive
connection con_super;
SELECT USER();

--echo # Verify that the additional super_session is kept alive
connection con_super1;
SELECT USER();

--echo # Verify that the sysadmin_session is kept alive
connection con_sysadmin;
SELECT USER();

--echo
--echo ## TEST 2: sysadmin_session activates OFFLINE_MODE
--echo

--echo # Setup - activate super_session connection
connection con_super;

--echo # Setup - deactivate OFFLINE_MODE
SET GLOBAL OFFLINE_MODE=OFF;

--echo # Setup - create additional regular_session connection
connect (con_regular1, localhost, regular, regular, );

--echo # Activate sysadmin_session connection
connection con_sysadmin;

--echo # Activate OFFLINE_MODE (with super_session)
SET GLOBAL OFFLINE_MODE=ON;

--echo # Verify that the sysadmin_session did not kill itself
SELECT USER();

--echo # Verify that the regular_session has been killed
connection con_regular1;
--error CR_SERVER_LOST
SELECT USER();

--echo # Verify that the power_session has been killed
connection con_power;
--error CR_SERVER_LOST
SELECT USER();

--echo # Verify that the super_session is kept alive
connection con_super;
SELECT USER();

## Cleanup
--echo
--echo # CLEAN UP
--echo
connection default;
disconnect con_super;
disconnect con_super1;
disconnect con_regular;
disconnect con_regular1;
disconnect con_power;
disconnect con_sysadmin;
DROP USER regular@localhost;
DROP USER power@localhost;
DROP USER super@localhost;
DROP USER sysadmin@localhost;
SET @@global.offline_mode = @original_offline_mode;
