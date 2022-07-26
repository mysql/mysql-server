# Create an empty data directory...
# Unzip 5_7_9_clean.zip to the created directory...
# Kill the server
############################################################################################
# Case 3: Attempt to start with ib_logfile0 which has corrupted file header (checksum).
# Pre-conditions: logically empty redo files from mysql-5.7.9,
#                 with corrupted checksum of the ib_logfile0's header
# 3A: InnoDB refuses to start and emits error about corrupted file header
# NOTE: Ensure there was no attempt to check if redo is empty.
############################################################################################
# Case 3A: Start MySQL...
# Verify...
Pattern "Invalid redo log header checksum" found
Pattern "Failed to read header of the redo log file .*ib_logfile0" found
Pattern "Upgrading redo log: .*, LSN=" not found
Pattern "innodb-unknown-compound-v" not found
############################################################################################
# Cleanup...
# restart
