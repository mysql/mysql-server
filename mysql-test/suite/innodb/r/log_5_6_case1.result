# Create an empty data directory...
# Unzip 5_6_clean.zip to the created directory...
# Kill the server
############################################################################################
# Case 1: Attempt to start after clean shutdown on mysql-5.6.
# Pre-conditions: logically empty redo files from mysql-5.6.
# 1A: InnoDB refuses to start because of the too old redo log format.
# 1B: InnoDB refuses to start with innodb-force-recovery = 1 or 6.
# 1C: InnoDB refuses to start with innodb-read-only = ON.
############################################################################################
# Case 1A: Start MySQL... (attempt 1)
# Verify...
Pattern "Unsupported redo log format \(v0\)\. The redo log was created before MySQL 5\.7\.9" found
Pattern "Redo log is from an earlier version" not found
Pattern "The latest found checkpoint is at lsn =" not found
Pattern "Upgrading redo log: .*, LSN=" not found
Pattern "innodb-unknown-compound-v" not found
# Case 1A: Start MySQL... (attempt 2)
# Verify...
Pattern "Unsupported redo log format \(v0\)\. The redo log was created before MySQL 5\.7\.9" found
Pattern "Redo log is from an earlier version" not found
Pattern "The latest found checkpoint is at lsn =" not found
Pattern "Upgrading redo log: .*, LSN=" not found
Pattern "innodb-unknown-compound-v" not found
############################################################################################
# Case 1B-1: Start MySQL with --innodb-force-recovery=1...
# Verify...
Pattern "Database upgrade cannot be accomplished with innodb_force_recovery > 0" found
Pattern "Cannot upgrade format .* of redo log files when innodb-force-recovery > 0" not found
Pattern "The latest found checkpoint is at lsn =" not found
# Case 1B-2: Start MySQL with --innodb-force-recovery=6...
# Verify...
Pattern "Database upgrade cannot be accomplished with innodb_force_recovery > 0" found
Pattern "Cannot upgrade format .* of redo log files when innodb-force-recovery > 0" not found
Pattern "The latest found checkpoint is at lsn =" not found
############################################################################################
# Case 1C: Start MySQL with innodb-read-only=ON...
# Verify...
Pattern "Database upgrade cannot be accomplished in read-only mode" found
Pattern "The latest found checkpoint is at lsn =" not found
############################################################################################
# Cleanup...
# restart
