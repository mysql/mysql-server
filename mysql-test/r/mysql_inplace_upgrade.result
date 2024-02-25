###########################################################################
# Test check and repair
###########################################################################
CREATE DATABASE db1;
CREATE DATABASE db2;
CREATE TABLE db1.t1 (a INT) ENGINE=MYISAM;
# restart: --upgrade=FORCE --debug=+d,force_fix_user_schemas --log-error=MYSQLD_LOG
Pattern "Table 'db1.t1' requires repair." found
Pattern "Table 'db1.t1' repair failed." found
DROP DATABASE db1;
DROP DATABASE db2;
###########################################################################
# Stop the default mtr server
###########################################################################
# Stop DB server which was created by MTR default
###########################################################################
# Setup 8.0 data dir
###########################################################################
# Set different paths for --datadir
# Copy the remote tablespace & DB zip files from suite location to working location.
# Check that the file exists in the working folder.
# Unzip the zip file.
###########################################################################
# Abort server startup if upgrade is pending
###########################################################################
Pattern "Server upgrade started with version \d+, but server upgrade of version \d+ is still pending\." found
Pattern "Server upgrade started with version \d+, but server upgrade of version \d+ is still pending\." found
###########################################################################
# Test default upgrade option
###########################################################################
Pattern "upgrade\s+AUTO" found
Pattern "option \'--upgrade\' requires an argument\." found
###########################################################################
# Cleanup
###########################################################################
# restart:
