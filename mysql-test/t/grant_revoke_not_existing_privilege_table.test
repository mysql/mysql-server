###############################################################################
# Test errors while granting and revoking privilege with respect to prior     #
# existence of the privilege.                                                 #
# Tests for table level and column level privileges.                          #
#                                                                             #
# Bug#34063709 Lack of error while revoking nonexistent privilege.            #
###############################################################################

# Prepare
CREATE USER rngp_user;
CREATE ROLE rngp_role;
CREATE DATABASE rngp_db;
CREATE TABLE rngp_db.tb1 (x INT);
CREATE TABLE rngp_db.tb2 (no INT, name VARCHAR(20));

####### Tests with REVOKE on specific privileges -role

# Revoke non existent, no privileges -issue error
--error ER_NONEXISTING_TABLE_GRANT
REVOKE REFERENCES ON rngp_db.tb1 FROM rngp_role;
--error ER_NONEXISTING_TABLE_GRANT
REVOKE REFERENCES (no) ON rngp_db.tb2 FROM rngp_role;

# Grant new privileges -OK
GRANT SELECT, INSERT ON rngp_db.tb1 TO rngp_role;
GRANT SELECT (no), INSERT (no) ON rngp_db.tb2 TO rngp_role;
SHOW GRANTS FOR rngp_role;
--echo

# Grant already existing privilege -OK, but no change
GRANT SELECT ON rngp_db.tb1 TO rngp_role;
GRANT SELECT (no) ON rngp_db.tb2 TO rngp_role;
SHOW GRANTS FOR rngp_role;
--echo

# Grant already existing privilege altogether with new one -OK
GRANT SELECT, UPDATE ON rngp_db.tb1 TO rngp_role;
GRANT SELECT (no), UPDATE (no) ON rngp_db.tb2 TO rngp_role;
SHOW GRANTS FOR rngp_role;
--echo

# Revoke existent, -OK
REVOKE INSERT ON rngp_db.tb1 FROM rngp_role;
REVOKE INSERT (no) ON rngp_db.tb2 FROM rngp_role;
SHOW GRANTS FOR rngp_role;
--echo

# Revoke both existent and not existent privileges at one time
--error ER_NONEXISTING_TABLE_GRANT
REVOKE SELECT, INSERT ON rngp_db.tb1 FROM rngp_role;
--error ER_NONEXISTING_TABLE_GRANT
REVOKE SELECT (no), INSERT (no) ON rngp_db.tb2 FROM rngp_role;

# Revoke non existent, other privileges exist -issue error
--error ER_NONEXISTING_TABLE_GRANT
REVOKE REFERENCES ON rngp_db.tb1 FROM rngp_role;
--error ER_NONEXISTING_TABLE_GRANT
REVOKE REFERENCES (no) ON rngp_db.tb2 FROM rngp_role;

# Same as above, but test compatibility with older version -no error
SET original_server_version := 80200;
REVOKE REFERENCES ON rngp_db.tb1 FROM rngp_role;
SET original_server_version := 80200;
REVOKE REFERENCES (no) ON rngp_db.tb2 FROM rngp_role;

# Set original_server_version to the current -issue error
SET @current_version := CAST(
   SUBSTRING_INDEX(@@GLOBAL.version, '.', 1)*10000
   +SUBSTRING_INDEX(SUBSTRING_INDEX(@@GLOBAL.version, '.', 2), '.', -1)*100
   +SUBSTRING_INDEX(SUBSTRING_INDEX(@@GLOBAL.version, '-', 1), '.', -1)
   AS UNSIGNED);
SET @@session.original_server_version := @current_version;
--error ER_NONEXISTING_TABLE_GRANT
REVOKE REFERENCES ON rngp_db.tb1 FROM rngp_role;
--error ER_NONEXISTING_TABLE_GRANT
REVOKE REFERENCES (no) ON rngp_db.tb2 FROM rngp_role;

# Check, that none of the above REVOKEs changed grants for rngp_role
SHOW GRANTS FOR rngp_role;
--echo

# Revoke existent -OK
REVOKE UPDATE ON rngp_db.tb1 FROM rngp_role;
REVOKE UPDATE (no) ON rngp_db.tb2 FROM rngp_role;
SHOW GRANTS FOR rngp_role;
--echo

# Revoke existent with IF EXISTS clause -OK with no warnings
REVOKE IF EXISTS SELECT ON rngp_db.tb1 FROM rngp_role;
SHOW WARNINGS;
REVOKE IF EXISTS SELECT (no) ON rngp_db.tb2 FROM rngp_role;
SHOW WARNINGS;
SHOW GRANTS FOR rngp_role;
--echo

# Grant back SELECT
GRANT SELECT ON rngp_db.tb1 TO rngp_role;
GRANT SELECT (no) ON rngp_db.tb2 TO rngp_role;

# Revoke non existent and existent with IF EXISTS clause
# -issue warning, but do remove the other privilege
REVOKE IF EXISTS INSERT, SELECT ON rngp_db.tb1 FROM rngp_role;
SHOW WARNINGS;
SHOW GRANTS FOR rngp_role;
REVOKE IF EXISTS INSERT (no), SELECT (no) ON rngp_db.tb2 FROM rngp_role;
SHOW WARNINGS;
SHOW GRANTS FOR rngp_role;
--echo


# Grant back INSERT
GRANT INSERT ON rngp_db.tb1 TO rngp_role;
GRANT INSERT (no) ON rngp_db.tb2 TO rngp_role;
SHOW GRANTS FOR rngp_role;
--echo

# Revoke non existent and existent with IF EXISTS clause
# as above, but in compatibility mode
# -no warning and do remove the other privilege
SET original_server_version := 80200;
REVOKE IF EXISTS INSERT, SELECT ON rngp_db.tb1 FROM rngp_role;
SHOW WARNINGS;
SHOW GRANTS FOR rngp_role;
SET original_server_version := 80200;
REVOKE IF EXISTS INSERT (no), SELECT (no) ON rngp_db.tb2 FROM rngp_role;
SHOW WARNINGS;
SHOW GRANTS FOR rngp_role;
--echo

SET @@session.original_server_version := @current_version;

####### Tests with REVOKE on specific privileges, mixed column and table -role

# Revoke non existent, no privileges -issue error
--error ER_NONEXISTING_TABLE_GRANT
REVOKE UPDATE, REFERENCES (x) ON rngp_db.tb1 FROM rngp_role;

# Grant new privileges -OK
GRANT INSERT, SELECT (x) ON rngp_db.tb1 TO rngp_role;
SHOW GRANTS FOR rngp_role;
--echo

# Grant already existing privilege -OK, but no change
GRANT INSERT, SELECT (x) ON rngp_db.tb1 TO rngp_role;
SHOW GRANTS FOR rngp_role;
--echo

# Grant already existing privilege altogether with new one -OK
GRANT UPDATE, INSERT, SELECT (x), REFERENCES (x) ON rngp_db.tb1 TO rngp_role;
SHOW GRANTS FOR rngp_role;
--echo

# Revoke existent, -OK
REVOKE INSERT, SELECT (x) ON rngp_db.tb1 FROM rngp_role;
SHOW GRANTS FOR rngp_role;
--echo

# Revoke both existent and not existent privileges at one time
--error ER_NONEXISTING_TABLE_GRANT
REVOKE UPDATE, INSERT, SELECT (x), REFERENCES (x) ON rngp_db.tb1 FROM rngp_role;
SHOW GRANTS FOR rngp_role;
--echo

# Revoke non existent, other privileges exist -issue error
--error ER_NONEXISTING_TABLE_GRANT
REVOKE INSERT, SELECT (x) ON rngp_db.tb1 FROM rngp_role;
SHOW GRANTS FOR rngp_role;
--echo

# Revoke if exist both existent and not existent privileges at one time
REVOKE IF EXISTS UPDATE, INSERT, SELECT (x), REFERENCES (x) ON rngp_db.tb1 FROM rngp_role;
SHOW WARNINGS;
SHOW GRANTS FOR rngp_role;
--echo

####### Tests with REVOKE on specific privileges -user

# Revoke non existent, no privileges -issue error
--error ER_NONEXISTING_TABLE_GRANT
REVOKE REFERENCES ON rngp_db.tb1 FROM rngp_user;
--error ER_NONEXISTING_TABLE_GRANT
REVOKE REFERENCES (no) ON rngp_db.tb2 FROM rngp_user;

# Grant new privileges -OK
GRANT SELECT, INSERT ON rngp_db.tb1 TO rngp_user;
GRANT SELECT (no), INSERT (no) ON rngp_db.tb2 TO rngp_user;
SHOW GRANTS FOR rngp_user;
--echo

# Grant already existing privilege -OK, but no change
GRANT SELECT ON rngp_db.tb1 TO rngp_user;
GRANT SELECT (no) ON rngp_db.tb2 TO rngp_user;
SHOW GRANTS FOR rngp_user;
--echo

# Grant already existing privilege altogether with new one -OK
GRANT SELECT, UPDATE ON rngp_db.tb1 TO rngp_user;
GRANT SELECT (no), UPDATE (no) ON rngp_db.tb2 TO rngp_user;
SHOW GRANTS FOR rngp_user;
--echo

# Revoke existent, -OK
REVOKE INSERT ON rngp_db.tb1 FROM rngp_user;
REVOKE INSERT (no) ON rngp_db.tb2 FROM rngp_user;
SHOW GRANTS FOR rngp_user;
--echo

# Revoke both existent and not existent privileges at one time
--error ER_NONEXISTING_TABLE_GRANT
REVOKE SELECT, INSERT ON rngp_db.tb1 FROM rngp_user;
--error ER_NONEXISTING_TABLE_GRANT
REVOKE SELECT (no), INSERT (no) ON rngp_db.tb2 FROM rngp_user;

# Revoke non existent, other privileges exist -issue error
--error ER_NONEXISTING_TABLE_GRANT
REVOKE REFERENCES ON rngp_db.tb1 FROM rngp_user;
--error ER_NONEXISTING_TABLE_GRANT
REVOKE REFERENCES (no) ON rngp_db.tb2 FROM rngp_user;

# Same as above, but test compatibility with older version -no error
SET original_server_version := 80200;
REVOKE REFERENCES ON rngp_db.tb1 FROM rngp_user;
SET original_server_version := 80200;
REVOKE REFERENCES (no) ON rngp_db.tb2 FROM rngp_user;

# Set original_server_version to the current -issue error
SET @@session.original_server_version := @current_version;
--error ER_NONEXISTING_TABLE_GRANT
REVOKE REFERENCES ON rngp_db.tb1 FROM rngp_user;
--error ER_NONEXISTING_TABLE_GRANT
REVOKE REFERENCES (no) ON rngp_db.tb2 FROM rngp_user;

# Check, that none of the above REVOKEs changed grants for rngp_user
SHOW GRANTS FOR rngp_user;
--echo

# Revoke existent -OK
REVOKE UPDATE ON rngp_db.tb1 FROM rngp_user;
REVOKE UPDATE (no) ON rngp_db.tb2 FROM rngp_user;
SHOW GRANTS FOR rngp_user;
--echo

# Revoke existent with IF EXISTS clause -OK with no warnings
REVOKE IF EXISTS SELECT ON rngp_db.tb1 FROM rngp_user;
SHOW WARNINGS;
REVOKE IF EXISTS SELECT (no) ON rngp_db.tb2 FROM rngp_user;
SHOW WARNINGS;
SHOW GRANTS FOR rngp_user;
--echo

# Grant back SELECT
GRANT SELECT ON rngp_db.tb1 TO rngp_user;
GRANT SELECT (no) ON rngp_db.tb2 TO rngp_user;
SHOW GRANTS FOR rngp_user;

# Revoke non existent and existent with IF EXISTS clause
# -issue warning, but do remove the other privilege
REVOKE IF EXISTS INSERT, SELECT ON rngp_db.tb1 FROM rngp_user;
SHOW WARNINGS;
REVOKE IF EXISTS INSERT (no), SELECT (no) ON rngp_db.tb2 FROM rngp_user;
SHOW WARNINGS;
SHOW GRANTS FOR rngp_user;
--echo

# Grant back INSERT
GRANT INSERT ON rngp_db.tb1 TO rngp_user;
GRANT INSERT (no) ON rngp_db.tb2 TO rngp_user;
SHOW GRANTS FOR rngp_user;
--echo

# Revoke non existent and existent with IF EXISTS clause
# as above, but in compatibility mode
# -no warning and do remove the other privilege
SET original_server_version := 80200;
REVOKE IF EXISTS INSERT, SELECT ON rngp_db.tb1 FROM rngp_user;
SHOW WARNINGS;
SET original_server_version := 80200;
REVOKE IF EXISTS INSERT (no), SELECT (no) ON rngp_db.tb2 FROM rngp_user;
SHOW WARNINGS;
SHOW GRANTS FOR rngp_user;
--echo

SET @@session.original_server_version := @current_version;

####### Tests with REVOKE ALL -role

# REVOKE ALL PRIVILEGES ON table, no privileges on on that table -error
--error ER_NONEXISTING_TABLE_GRANT
REVOKE ALL PRIVILEGES ON rngp_db.tb1 FROM rngp_role;

# Grant new privileges -OK
GRANT SELECT, INSERT ON rngp_db.tb1 TO rngp_role;
SHOW GRANTS FOR rngp_role;
--echo

# REVOKE ALL on table, privileges exist and are removed -OK
REVOKE ALL PRIVILEGES ON rngp_db.tb1 FROM rngp_role;
SHOW GRANTS FOR rngp_role;
--echo

####### Tests with REVOKE ALL -user

# REVOKE ALL PRIVILEGES ON table, no privileges on on that table -error
--error ER_NONEXISTING_TABLE_GRANT
REVOKE ALL PRIVILEGES ON rngp_db.tb1 FROM rngp_user;

# Grant new privileges -OK
GRANT SELECT, INSERT ON rngp_db.tb1 TO rngp_user;
SHOW GRANTS FOR rngp_user;
--echo

# REVOKE ALL on table, privileges exist and are removed -OK
REVOKE ALL PRIVILEGES ON rngp_db.tb1 FROM rngp_user;
SHOW GRANTS FOR rngp_user;
--echo

# Cleanup
SET @current_version := NULL;
DROP ROLE rngp_role;
DROP USER rngp_user;
DROP TABLE rngp_db.tb1;
DROP TABLE rngp_db.tb2;
DROP DATABASE rngp_db;
