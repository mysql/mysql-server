#
# This test upgrades a database from 8.0.21 and attempts to add
# autoextend_size tablespace attribute to it's tables
# and tablespaces
#
# Stop the running server
###################################################################
# Unzip a 8.0.21 database and start the server using this directory
###################################################################
###################################################################
# Copy and unzip the datadir
###################################################################
# Restart the server using the new datadir
# restart: --datadir=BUGDATADIR --innodb_page_size=16k
# Check the contents of INFORMATION_SCHEMA.INNODB_TABLESPACES
SELECT lower(name), file_size, autoextend_size from
INFORMATION_SCHEMA.INNODB_TABLESPACES where name like '%t%';
lower(name)	file_size	autoextend_size
innodb_temporary	12582912	0
test/t5	114688	0
ts1	147456	0
# Update autoextend_size attribute both for the implicit as
# well as explicit tablespaces
ALTER TABLE t5 AUTOEXTEND_SIZE 4M;
ALTER TABLESPACE ts1 AUTOEXTEND_SIZE 4M;
# Check the contents of INFORMATION_SCHEMA.INNODB_TABLESPACES
SELECT lower(name), file_size, autoextend_size from
INFORMATION_SCHEMA.INNODB_TABLESPACES where name like '%t%';
lower(name)	file_size	autoextend_size
innodb_temporary	12582912	0
test/t5	114688	4194304
ts1	147456	4194304
# Cleanup
# Shutdown server
# Remove the copied files
# Restart the server
# restart
