--echo ##
--echo ## WL#17202: Allow/disallow asynchronous replication from a higher version source
--echo ##
--echo ## For different clients, test that REPLICA_ALLOW_HIGHER_VERSION_SOURCE can only be modified by those
--echo ## who have REPLICATION_SLAVE_ADMIN plus SYSTEM_VARIABLES_ADMIN or SUPER privileges.
--echo ## Of the following, only those with + prefix should be able to modify REPLICA_ALLOW_HIGHER_VERSION_SOURCE:
--echo ## - base_session:       having no privileges
--echo ## - regular_session:    having SYSTEM_VARIABLES_ADMIN privilege
--echo ## - rpl_admin_session:  having REPLICATION_SLAVE_ADMIN privilege
--echo ## - power_session:      having SYSTEM_VARIABLES_ADMIN and SYSTEM_USER privileges
--echo ## + super_session:      having SUPER privileges
--echo ## + admin_session:      having SYSTEM_VARIABLES_ADMIN and REPLICATION_SLAVE_ADMIN privileges
--echo ## + superadmin_session: having SYSTEM_VARIABLES_ADMIN and SUPER and REPLICATION_SLAVE_ADMIN privileges
--echo ## + sysadmin_session:   having SYSTEM_VARIABLES_ADMIN, REPLICATION_SLAVE_ADMIN, SYSTEM_USER privileges
--echo ##

SET @original_REPLICA_ALLOW_HIGHER_VERSION_SOURCE = @@global.REPLICA_ALLOW_HIGHER_VERSION_SOURCE;

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
--echo # Create rpl_admin_session user
CREATE USER rpl_admin@localhost IDENTIFIED BY 'rpl_admin';
GRANT REPLICATION_SLAVE_ADMIN ON *.* TO rpl_admin@localhost;

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
GRANT SYSTEM_VARIABLES_ADMIN, REPLICATION_SLAVE_ADMIN ON *.* TO admin@localhost;

--echo
--echo # Create superadmin_session user
CREATE USER superadmin@localhost IDENTIFIED BY 'superadmin';
GRANT SYSTEM_VARIABLES_ADMIN, REPLICATION_SLAVE_ADMIN, SUPER ON *.* TO superadmin@localhost;

--echo
--echo # Create sysadmin_session user (max privileges)
CREATE USER sysadmin@localhost IDENTIFIED BY 'sysadmin';
GRANT SYSTEM_VARIABLES_ADMIN, REPLICATION_SLAVE_ADMIN, SYSTEM_USER ON *.* TO sysadmin@localhost;

--echo
--echo ## TEST 1: base_session can not modify REPLICA_ALLOW_HIGHER_VERSION_SOURCE
--echo

connect (con_base, localhost, base, base, );
--error ER_SPECIFIC_ACCESS_DENIED_ERROR
SET GLOBAL REPLICA_ALLOW_HIGHER_VERSION_SOURCE=ON;
disconnect con_base;

--echo
--echo ## TEST 2: regular_session can not modify REPLICA_ALLOW_HIGHER_VERSION_SOURCE
--echo

connect (con_regular, localhost, regular, regular, );
--error ER_SPECIFIC_ACCESS_DENIED_ERROR
SET GLOBAL REPLICA_ALLOW_HIGHER_VERSION_SOURCE=ON;
disconnect con_regular;

--echo
--echo ## TEST 3: rpl_admin_session can not modify REPLICA_ALLOW_HIGHER_VERSION_SOURCE
--echo

connect (con_rpl_admin, localhost, rpl_admin, rpl_admin, );
--error ER_SPECIFIC_ACCESS_DENIED_ERROR
SET GLOBAL REPLICA_ALLOW_HIGHER_VERSION_SOURCE=ON;
disconnect con_rpl_admin;

--echo
--echo ## TEST 4: power_session can not modify REPLICA_ALLOW_HIGHER_VERSION_SOURCE
--echo

connect (con_power, localhost, power, power, );
--error ER_SPECIFIC_ACCESS_DENIED_ERROR
SET GLOBAL REPLICA_ALLOW_HIGHER_VERSION_SOURCE=ON;
disconnect con_power;

--echo
--echo ## TEST 5: super_session can modify REPLICA_ALLOW_HIGHER_VERSION_SOURCE
--echo

connect (con_super, localhost, super, super, );
SET GLOBAL REPLICA_ALLOW_HIGHER_VERSION_SOURCE=ON;
disconnect con_super;

--echo
--echo ## TEST 6: admin_session can modify REPLICA_ALLOW_HIGHER_VERSION_SOURCE
--echo

connect (con_admin, localhost, admin, admin, );
SET GLOBAL REPLICA_ALLOW_HIGHER_VERSION_SOURCE=OFF;
disconnect con_admin;

--echo
--echo ## TEST 7: superadmin_session can modify REPLICA_ALLOW_HIGHER_VERSION_SOURCE
--echo

connect (con_superadmin, localhost, superadmin, superadmin, );
SET GLOBAL REPLICA_ALLOW_HIGHER_VERSION_SOURCE=ON;
disconnect con_superadmin;

--echo
--echo ## TEST 8: sysadmin_session can modify REPLICA_ALLOW_HIGHER_VERSION_SOURCE
--echo

connect (con_sysadmin, localhost, sysadmin, sysadmin, );
SET GLOBAL REPLICA_ALLOW_HIGHER_VERSION_SOURCE=OFF;
disconnect con_sysadmin;

## Cleanup
--echo
--echo # CLEAN UP
--echo
connection default;
DROP USER base@localhost;
DROP USER regular@localhost;
DROP USER rpl_admin@localhost;
DROP USER power@localhost;
DROP USER super@localhost;
DROP USER admin@localhost;
DROP USER superadmin@localhost;
DROP USER sysadmin@localhost;
SET @@global.REPLICA_ALLOW_HIGHER_VERSION_SOURCE = @original_REPLICA_ALLOW_HIGHER_VERSION_SOURCE;
