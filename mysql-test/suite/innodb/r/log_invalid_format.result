# Create new data directory (in order to corrupt its redo log files)...
############################################################################################
# Case 1. Attempt to start with redo files in old location, filled with 0x00.
############################################################################################
# Prepare...
# Start MySQL... (attempt 1)
# Verify...
Pattern "Unsupported redo log format \(v0\)" found
Pattern "innodb-unknown-compound-v" not found
# Start MySQL... (attempt 2)
# Verify...
Pattern "Unsupported redo log format \(v0\)" found
Pattern "innodb-unknown-compound-v" not found
############################################################################################
# Case 2. Attempt to start with redo files in new location, filled with 0x00.
############################################################################################
# Prepare...
# Start MySQL... (attempt 1)
# Verify...
Pattern "Found redo log file.*#ib_redo1 which has format \(v0\) and is stored inside #innodb_redo" found
Pattern "innodb-unknown-compound-v" not found
# Start MySQL... (attempt 2)
# Verify...
Pattern "Found redo log file.*#ib_redo1 which has format \(v0\) and is stored inside #innodb_redo" found
Pattern "innodb-unknown-compound-v" not found
############################################################################################
# Case 3. Attempt to start with redo files in old location, with future format (4294967295).
############################################################################################
# Prepare...
# Start MySQL... (attempt 1)
# Verify...
Pattern "Found redo log file .*ib_logfile0 which has format \(v4294967295\) and is stored outside #innodb_redo" found
Pattern "innodb-unknown-compound-v" not found
# Start MySQL... (attempt 2)
# Verify...
Pattern "Found redo log file .*ib_logfile0 which has format \(v4294967295\) and is stored outside #innodb_redo" found
Pattern "innodb-unknown-compound-v" not found
############################################################################################
# Case 4. Attempt to start with redo files in new location, with future format (4294967295).
############################################################################################
# Prepare...
# Start MySQL... (attempt 1)
# Verify...
Pattern "Unknown redo log format \(v4294967295\) in file.*#ib_redo1" found
Pattern "innodb-unknown-compound-v" not found
# Start MySQL... (attempt 2)
# Verify...
Pattern "Unknown redo log format \(v4294967295\) in file.*#ib_redo1" found
Pattern "innodb-unknown-compound-v" not found
############################################################################################
# Cleanup...
