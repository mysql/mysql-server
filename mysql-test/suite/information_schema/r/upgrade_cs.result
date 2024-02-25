########################################################################
# This test scripts covers meta data related aspects of upgrade
# after 8.0.
########################################################################
#-----------------------------------------------------------------------
# WL#10895: INFORMATION_SCHEMA views for Roles
# Test case to verify that new I_S tables introduced by this WL
# are present after upgrade.
#-----------------------------------------------------------------------
# Copy the remote tablespace & DB zip files from suite location to working location.
# Check that the file exists in the working folder.
# Set different paths for --datadir
# Check that the file exits after unzip
# Stop / restart DB server which was created by MTR default
# restart: --datadir=MYSQLD_DATADIR1 --log-error=MYSQLD_LOG
SELECT COUNT(*) < 0 FROM INFORMATION_SCHEMA.APPLICABLE_ROLES;
COUNT(*) < 0
0
SELECT COUNT(*) < 0 FROM INFORMATION_SCHEMA.ADMINISTRABLE_ROLE_AUTHORIZATIONS;
COUNT(*) < 0
0
SELECT COUNT(*) < 0 FROM INFORMATION_SCHEMA.ENABLED_ROLES;
COUNT(*) < 0
0
SELECT COUNT(*) < 0 FROM INFORMATION_SCHEMA.ROLE_TABLE_GRANTS;
COUNT(*) < 0
0
SELECT COUNT(*) < 0 FROM INFORMATION_SCHEMA.ROLE_COLUMN_GRANTS;
COUNT(*) < 0
0
SELECT COUNT(*) < 0 FROM INFORMATION_SCHEMA.ROLE_ROUTINE_GRANTS;
COUNT(*) < 0
0
# Stop the server and do cleanup
# Remove copied files
# Restart the server with default options.
# restart:
#
# WL#10895 Upgrade from 5.7->8.0 should show I_S tables.
#
#
# Copy zip files from suite location to working location.
# Check that the file exists in the working folder.
# Unzip the zip file.
# Set different paths for --datadir
# Stop DB server which was created by MTR default
# Start the DB server to do upgrade.
SELECT COUNT(*) < 0 FROM INFORMATION_SCHEMA.APPLICABLE_ROLES;
COUNT(*) < 0
0
SELECT COUNT(*) < 0 FROM INFORMATION_SCHEMA.ADMINISTRABLE_ROLE_AUTHORIZATIONS;
COUNT(*) < 0
0
SELECT COUNT(*) < 0 FROM INFORMATION_SCHEMA.ENABLED_ROLES;
COUNT(*) < 0
0
SELECT COUNT(*) < 0 FROM INFORMATION_SCHEMA.ROLE_TABLE_GRANTS;
COUNT(*) < 0
0
SELECT COUNT(*) < 0 FROM INFORMATION_SCHEMA.ROLE_COLUMN_GRANTS;
COUNT(*) < 0
0
SELECT COUNT(*) < 0 FROM INFORMATION_SCHEMA.ROLE_ROUTINE_GRANTS;
COUNT(*) < 0
0
# Stop the server
# Remove copied files
# Restart the server with default options.
# restart:
