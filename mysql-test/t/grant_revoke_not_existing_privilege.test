###############################################################################
# Test errors while granting and revoking privilege with respect to prior     #
# existence of the privilege.                                                 #
#                                                                             #
# Bug #33897859 Unexpected behaviour seen with revoking a privilege from role.#
###############################################################################

# Prepare
CREATE ROLE rngp_role1, rngp_role2, rngp_role3;
CREATE USER rngp_user1, rngp_user2, rngp_user3;
CREATE DATABASE rngp_db;

SET @saved_partial_revokes = @@global.partial_revokes;
--disable_warnings
SET GLOBAL partial_revokes= OFF;
--enable_warnings


####### Tests with REVOKE on specific privileges and user

# Revoke non existent, no privileges -issue error
--error ER_NONEXISTING_GRANT
REVOKE DELETE ON rngp_db.* FROM rngp_user1;

# Grant new privileges -OK
GRANT SELECT, CREATE ON rngp_db.* TO rngp_user1;
SHOW GRANTS FOR rngp_user1;
--echo

# Grant already existing privilege -OK, but no change
GRANT SELECT ON rngp_db.* TO rngp_user1;
SHOW GRANTS FOR rngp_user1;
--echo

# Grant already existing privilege altogether with new one -OK
GRANT SELECT, UPDATE ON rngp_db.* TO rngp_user1;
SHOW GRANTS FOR rngp_user1;
--echo

# Revoke existent, -OK
REVOKE UPDATE ON rngp_db.* FROM rngp_user1;

# Revoke both existent and not existent privileges at one time
--error ER_NONEXISTING_GRANT
REVOKE SELECT, INSERT ON rngp_db.* FROM rngp_user1;

# Revoke non existent, other privileges exist -issue error
--error ER_NONEXISTING_GRANT
REVOKE DELETE ON rngp_db.* FROM rngp_user1;

# Same as above, but test compatibility with older version -no error
SET original_server_version := 80200;
REVOKE DELETE ON rngp_db.* FROM rngp_user1;

# Set original_server_version to the current -issue error
SET @@session.original_server_version := CAST(
   SUBSTRING_INDEX(@@GLOBAL.version, '.', 1)*10000
   +SUBSTRING_INDEX(SUBSTRING_INDEX(@@GLOBAL.version, '.', 2), '.', -1)*100
   +SUBSTRING_INDEX(SUBSTRING_INDEX(@@GLOBAL.version, '-', 1), '.', -1)
   AS UNSIGNED);
--error ER_NONEXISTING_GRANT
REVOKE DELETE ON rngp_db.* FROM rngp_user1;

# Check, that UPDATE was removed, but not SELECT
SHOW GRANTS FOR rngp_user1;
--echo

# Revoke existent -OK
REVOKE CREATE ON rngp_db.* FROM rngp_user1;
SHOW GRANTS FOR rngp_user1;
--echo

# Revoke existent with IF EXISTS clause -OK with warnings
REVOKE IF EXISTS SELECT ON rngp_db.* FROM rngp_user1;
SHOW WARNINGS;
SHOW GRANTS FOR rngp_user1;
--echo

# Revoke non existent and existent with IF EXISTS clause
# -issue warning, but do remove the other privilege
REVOKE IF EXISTS CREATE, SELECT ON rngp_db.* FROM rngp_user1;
SHOW WARNINGS;
SHOW GRANTS FOR rngp_user1;
--echo

####### Tests with REVOKE on specific privileges and role

# Revoke non existent, no privileges -issue error
--error ER_NONEXISTING_GRANT
REVOKE DELETE ON rngp_db.* FROM rngp_role1;

# Grant new privileges -OK
GRANT SELECT, CREATE ON rngp_db.* TO rngp_role1;
SHOW GRANTS FOR rngp_role1;
--echo

# Grant already existing privilege -OK, but no change
GRANT SELECT ON rngp_db.* TO rngp_role1;
SHOW GRANTS FOR rngp_role1;
--echo

# Grant already existing privilege altogether with new one -OK
GRANT SELECT, UPDATE ON rngp_db.* TO rngp_role1;
SHOW GRANTS FOR rngp_role1;
--echo

# Revoke existent, -OK
REVOKE UPDATE ON rngp_db.* FROM rngp_role1;

# Revoke both existent and not existent privileges at one time
--error ER_NONEXISTING_GRANT
REVOKE SELECT, INSERT ON rngp_db.* FROM rngp_role1;

# Revoke non existent, other privileges exist -issue error
--error ER_NONEXISTING_GRANT
REVOKE DELETE ON rngp_db.* FROM rngp_role1;

# Same as above, but test compatibility with older version -no error
SET original_server_version := 80200;
REVOKE DELETE ON rngp_db.* FROM rngp_role1;

# Set original_server_version to the current -issue error
SET @@session.original_server_version := CAST(
   SUBSTRING_INDEX(@@GLOBAL.version, '.', 1)*10000
   +SUBSTRING_INDEX(SUBSTRING_INDEX(@@GLOBAL.version, '.', 2), '.', -1)*100
   +SUBSTRING_INDEX(SUBSTRING_INDEX(@@GLOBAL.version, '-', 1), '.', -1)
   AS UNSIGNED);
--error ER_NONEXISTING_GRANT
REVOKE DELETE ON rngp_db.* FROM rngp_role1;

# Check, that UPDATE was removed, but not SELECT
SHOW GRANTS FOR rngp_role1;
--echo

# Revoke existent -OK
REVOKE CREATE ON rngp_db.* FROM rngp_role1;
SHOW GRANTS FOR rngp_role1;
--echo

# Revoke existent with IF EXISTS clause -OK with warnings
REVOKE IF EXISTS SELECT ON rngp_db.* FROM rngp_role1;
SHOW WARNINGS;
SHOW GRANTS FOR rngp_role1;
--echo

# Revoke non existent and existent with IF EXISTS clause
# -issue warning, but do remove the other privilege
REVOKE IF EXISTS CREATE, SELECT ON rngp_db.* FROM rngp_role1;
SHOW WARNINGS;
SHOW GRANTS FOR rngp_role1;
--echo

####### Tests with REVOKE ALL and user

# REVOKE ALL PRIVILEGES, GRANT OPTION, no privilege exist - always OK
REVOKE ALL PRIVILEGES, GRANT OPTION FROM rngp_user1;

# REVOKE ALL PRIVILEGES ON db, no privileges on on that db -error
--error ER_NONEXISTING_GRANT
REVOKE ALL PRIVILEGES ON rngp_db.* FROM rngp_user1;

# Grant new privileges -OK
GRANT SELECT, CREATE ON rngp_db.* TO rngp_user1;
SHOW GRANTS FOR rngp_user1;
--echo

# REVOKE ALL PRIVILEGES, GRANT OPTION, privileges exist -OK
REVOKE ALL PRIVILEGES, GRANT OPTION FROM rngp_user1;
SHOW GRANTS FOR rngp_user1;
--echo

# Grant new privileges -OK
GRANT SELECT, CREATE ON rngp_db.* TO rngp_user1;
SHOW GRANTS FOR rngp_user1;
--echo

# REVOKE ALL on db, privileges exist and are removed -OK
REVOKE ALL PRIVILEGES ON rngp_db.* FROM rngp_user1;
SHOW GRANTS FOR rngp_user1;
--echo

# REVOKE ALL on db, no privilege exist -error
--error ER_NONEXISTING_GRANT
REVOKE ALL PRIVILEGES ON rngp_db.* FROM rngp_user1;

####### Tests with REVOKE ALL and role

# REVOKE ALL PRIVILEGES, GRANT OPTION, no privilege exist - always OK
REVOKE ALL PRIVILEGES, GRANT OPTION FROM rngp_role1;

# REVOKE ALL PRIVILEGES ON db, no privileges on on that db -error
--error ER_NONEXISTING_GRANT
REVOKE ALL PRIVILEGES ON rngp_db.* FROM rngp_role1;

# Grant new privileges -OK
GRANT SELECT, CREATE ON rngp_db.* TO rngp_role1;
SHOW GRANTS FOR rngp_role1;
--echo

# REVOKE ALL PRIVILEGES, GRANT OPTION, privileges exist -OK
REVOKE ALL PRIVILEGES, GRANT OPTION FROM rngp_role1;
SHOW GRANTS FOR rngp_role1;
--echo

# Grant new privileges -OK
GRANT SELECT, CREATE ON rngp_db.* TO rngp_role1;
SHOW GRANTS FOR rngp_role1;
--echo

# REVOKE ALL on db, privileges exist and are removed -OK
REVOKE ALL PRIVILEGES ON rngp_db.* FROM rngp_role1;
SHOW GRANTS FOR rngp_role1;
--echo

# REVOKE ALL on db, no privilege exist -error
--error ER_NONEXISTING_GRANT
REVOKE ALL PRIVILEGES ON rngp_db.* FROM rngp_role1;

####### Tests with partial_revokes=ON and user

# set partial_revokes
--disable_warnings
SET GLOBAL partial_revokes= ON;
--enable_warnings


GRANT SELECT ON *.* TO rngp_user1;
GRANT SELECT ON rngp_db.* TO rngp_user1;

# revoke already granted privilege -OK
REVOKE SELECT ON rngp_db.* FROM rngp_user1;
SHOW GRANTS FOR rngp_user1;
--echo

# partial revoke -OK
REVOKE SELECT ON rngp_db.* FROM rngp_user1;
SHOW GRANTS FOR rngp_user1;
--echo

# partial revoke on already revoked -OK
REVOKE SELECT ON rngp_db.* FROM rngp_user1;
SHOW GRANTS FOR rngp_user1;
--echo

# reset privs
REVOKE ALL ON *.* FROM rngp_user1;
GRANT SELECT ON *.* TO rngp_user1;
GRANT SELECT, INSERT ON rngp_db.* TO rngp_user1;
SHOW GRANTS FOR rngp_user1;
--echo

# revoke specific privs -OK
REVOKE SELECT, INSERT ON rngp_db.* FROM rngp_user1;
SHOW GRANTS FOR rngp_user1;
--echo

# this is not partial revoke as there is no INSERT granted on *.*
--error ER_NONEXISTING_GRANT
REVOKE SELECT, INSERT ON rngp_db.* FROM rngp_user1;
SHOW GRANTS FOR rngp_user1;
--echo

# this is a correct partial revoke
REVOKE SELECT ON rngp_db.* FROM rngp_user1;
SHOW GRANTS FOR rngp_user1;
--echo

# this is not partial revoke as there is no INSERT granted on *.*
--error ER_NONEXISTING_GRANT
REVOKE INSERT ON rngp_db.* FROM rngp_user1;

# reset privs
REVOKE ALL ON *.* FROM rngp_user1;
GRANT SELECT, INSERT ON *.* to rngp_user1;

# cannot revoke priv for user that has no privs, the revoke fails for both users
--error ER_NONEXISTING_GRANT
REVOKE SELECT ON rngp_db.* FROM rngp_user1, rngp_user2;

# same with WITH EXISTS reports warning and do partial revoke for the first user
REVOKE IF EXISTS SELECT ON rngp_db.* FROM rngp_user1, rngp_user2;
SHOW GRANTS FOR rngp_user1;
--echo

# Revoking DB level privilege multiple times should raise error
# when it is not a partial revoke
GRANT ALL ON rngp_db.* TO rngp_user3;
REVOKE SELECT ON rngp_db.* FROM rngp_user3;

--error ER_NONEXISTING_GRANT
REVOKE SELECT ON rngp_db.* FROM rngp_user3;
SHOW GRANTS FOR rngp_user3;
--echo

####### Tests with partial_revokes=ON and role

GRANT SELECT ON *.* TO rngp_role1;
GRANT SELECT ON rngp_db.* TO rngp_role1;

# revoke already granted privilege -OK
REVOKE SELECT ON rngp_db.* FROM rngp_role1;
SHOW GRANTS FOR rngp_role1;
--echo

# partial revoke -OK
REVOKE SELECT ON rngp_db.* FROM rngp_role1;
SHOW GRANTS FOR rngp_role1;
--echo

# partial revoke on already revoked -OK
REVOKE SELECT ON rngp_db.* FROM rngp_role1;
SHOW GRANTS FOR rngp_role1;
--echo

# reset privs
REVOKE ALL ON *.* FROM rngp_role1;
GRANT SELECT ON *.* TO rngp_role1;
GRANT SELECT, INSERT ON rngp_db.* TO rngp_role1;
SHOW GRANTS FOR rngp_role1;
--echo

# revoke specific privs -OK
REVOKE SELECT, INSERT ON rngp_db.* FROM rngp_role1;
SHOW GRANTS FOR rngp_role1;
--echo

# this is not partial revoke as there is no INSERT granted on *.*
--error ER_NONEXISTING_GRANT
REVOKE SELECT, INSERT ON rngp_db.* FROM rngp_role1;
SHOW GRANTS FOR rngp_role1;
--echo

# this is a correct partial revoke
REVOKE SELECT ON rngp_db.* FROM rngp_role1;
SHOW GRANTS FOR rngp_role1;
--echo

# this is not partial revoke as there is no INSERT granted on *.*
--error ER_NONEXISTING_GRANT
REVOKE INSERT ON rngp_db.* FROM rngp_role1;

# reset privs
REVOKE ALL ON *.* FROM rngp_role1;
GRANT SELECT, INSERT ON *.* to rngp_role1;

# cannot revoke priv for user that has no privs, the revoke fails for both users
--error ER_NONEXISTING_GRANT
REVOKE SELECT ON rngp_db.* FROM rngp_role1, rngp_role2;

# same with WITH EXISTS reports warning and do partial revoke for the first user
REVOKE IF EXISTS SELECT ON rngp_db.* FROM rngp_role1, rngp_role2;
SHOW GRANTS FOR rngp_role1;
--echo

# Revoking DB level privilege multiple times should raise error
# when it is not a partial revoke
GRANT ALL ON rngp_db.* TO rngp_role3;
REVOKE SELECT ON rngp_db.* FROM rngp_role3;

--error ER_NONEXISTING_GRANT
REVOKE SELECT ON rngp_db.* FROM rngp_role3;
SHOW GRANTS FOR rngp_role3;
--echo


# unset partial_revokes flag, but first must revoke all partial revokes
REVOKE ALL ON *.* FROM rngp_user1;
REVOKE ALL ON *.* FROM rngp_role1;
--disable_warnings
SET GLOBAL partial_revokes = @saved_partial_revokes;
--enable_warnings

# Cleanup
DROP ROLE rngp_role1, rngp_role2, rngp_role3;
DROP USER rngp_user1, rngp_user2, rngp_user3;
DROP DATABASE rngp_db;
