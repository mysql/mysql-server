# Create an empty data directory...
# Unzip 5_7_9_clean.zip to the created directory...
# Kill the server
############################################################################################
# Case 7: Attempt to start on non-empty redo files.
# 7A: InnoDB refuses to start on logically non-empty redo log.
# 7B: InnoDB refuses to start when the first log block is corrupted (invalid checksum).
############################################################################################
# Update data_len field to 512 in the data block containing the checkpoint_lsn...
# Case 7A: Start MySQL... (attempt 1)
# Verify...
Pattern "Upgrade is not supported after a crash or shutdown with innodb_fast_shutdown = 2.* appears logically non empty" found
Pattern "Upgrading redo log: .*, LSN=" not found
Pattern "innodb-unknown-compound-v" not found
# Case 7A: Start MySQL... (attempt 2)
# Verify...
Pattern "Upgrade is not supported after a crash or shutdown with innodb_fast_shutdown = 2.* appears logically non empty" found
Pattern "Upgrading redo log: .*, LSN=" not found
Pattern "innodb-unknown-compound-v" not found
############################################################################################
# Case 7B: Start MySQL... (attempt 1)
# Verify...
Pattern "Upgrade after a crash is not supported.* appears corrupted" found
Pattern "Upgrading redo log: .*, LSN=" not found
Pattern "innodb-unknown-compound-v" not found
# Case 7B: Start MySQL... (attempt 2)
# Verify...
Pattern "Upgrade after a crash is not supported.* appears corrupted" found
Pattern "Upgrading redo log: .*, LSN=" not found
Pattern "innodb-unknown-compound-v" not found
############################################################################################
# Cleanup...
# restart
