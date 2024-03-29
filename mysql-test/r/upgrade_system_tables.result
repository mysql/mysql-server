# Create and initialize new data directory
# Restart server on the newly initialized data directory.
# restart: --datadir=MYSQLD_DATADIR --log-error=MYSQLD_ERROR_LOG

# Dump system table information for the newly initialized data directory
# Adding debug point 'skip_dd_table_access_check' to @@GLOBAL.debug

# Dump all tables in newly initialized mysql schema
include/write_result_to_file.inc [connection=default statement=SELECT t.* FROM mysql.tables t JOIN mysql.schemata s JOIN mysql.tablespaces ts WHERE t.schema_id = s.id AND t.tablespace_id = ts.id AND s.name = "mysql" AND ts.name = "mysql" ORDER BY t.name]
include/filter_file.inc

# Dump all columns in newly initialized mysql schema
include/write_result_to_file.inc [connection=default statement=SELECT t.name, c.* FROM mysql.columns c JOIN mysql.tables t JOIN mysql.schemata s JOIN mysql.tablespaces ts WHERE c.table_id = t.id AND t.schema_id = s.id AND t.tablespace_id = ts.id AND s.name = "mysql" AND ts.name = "mysql" AND t.name != "ndb_binlog_index" ORDER BY t.name, c.name]
include/filter_file.inc
# Removing debug point 'skip_dd_table_access_check' from @@GLOBAL.debug

# Upgrade a data directory from 5.7

# Copy the 5.7 data zip file to working directory and check it exists.
# Unzip 5.7 data directory.
# Set data directory and log file
# Restart server to trigger upgrade.
# restart: --datadir=MYSQLD_DATADIR1 --log-error=MYSQLD_LOG

# Dump system table information for the upgraded data directory
# Adding debug point 'skip_dd_table_access_check' to @@GLOBAL.debug

# Dump all tables in upgraded mysql schema
include/write_result_to_file.inc [connection=default statement=SELECT t.* FROM mysql.tables t JOIN mysql.schemata s JOIN mysql.tablespaces ts WHERE t.schema_id = s.id AND t.tablespace_id = ts.id AND s.name = "mysql" AND ts.name = "mysql" ORDER BY t.name]
include/filter_file.inc

# Dump all columns in upgraded mysql schema
include/write_result_to_file.inc [connection=default statement=SELECT t.name, c.* FROM mysql.columns c JOIN mysql.tables t JOIN mysql.schemata s JOIN mysql.tablespaces ts WHERE c.table_id = t.id AND t.schema_id = s.id AND t.tablespace_id = ts.id AND s.name = "mysql" AND ts.name = "mysql" AND t.name != "ndb_binlog_index" ORDER BY t.name, c.name]
include/filter_file.inc
# Removing debug point 'skip_dd_table_access_check' from @@GLOBAL.debug

########################################################################
# NOTE: If this test fails, there is a possible inconsistency introduced
#       in the system table/column definition of an upgraded mysql
#       tablespace as compared to that of a newly initialized one.
#
# There are 2 possibilities:
#
# a) If the failure shows a new inconsistency in the table/column
#    properties, add the appropriate ALTER TABLE statements to
#    scripts/mysql_system_tables_fix.sql
#    to ensure that the metadata of the upgraded table/column is the
#    same as that of the newly created one.
#
# b) If the inconsistency is unavoidable, modify the regular expressions
#    above, re-record and update documentation if required.
#
########################################################################

# Compare fresh and upgraded tables

# Compare fresh and upgraded columns

# Stop server and cleanup.
# Copy the 5.7 data zip file to working directory and check it exists.
# Unzip 5.7 data directory.
# Set data directory and log file
# Restart server to trigger upgrade.
# Upgrade of help tables should succeed.
# restart: --datadir=MYSQLD_DATADIR1 --log-error=MYSQLD_LOG
# Check for errors in the error log.
Pattern "\[ERROR\]" not found
# Check for mysql_upgrade_info file in data directory.
# Stop server and cleanup.
# Restart the server with default options.
# restart
