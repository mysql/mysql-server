#
# Show the affect of using --innodb-undo-directory
#
# Stop the MTR default mysqld since it was started with no --innodb-undo-directory
#
# Initialize a temporary database that uses a separate undo directory.
#
# Directory listing of MYSQL_UNDO_DIR
undo_001
undo_002
#
# Start the DB server with the temp datadir and undo directory
#
# restart: --datadir=MYSQL_DATA_DIR --innodb-undo-directory=MYSQL_UNDO_DIR --innodb-page-size=PAGE_SIZE
#
# Create 2 undo tablespaces in the undo directory
#
CREATE UNDO TABLESPACE undo_003 ADD DATAFILE 'undo_003.ibu';
CREATE UNDO TABLESPACE undo_004 ADD DATAFILE 'MYSQL_UNDO_DIR/undo_004.ibu';
#
# The following filename is exactly like the original datafile so this error is caught by MySQL.
#
CREATE UNDO TABLESPACE undo_999 ADD DATAFILE 'undo_003.ibu';
ERROR HY000: Duplicate file name for tablespace 'undo_999'
SHOW WARNINGS;
Level	Code	Message
Error	3606	Duplicate file name for tablespace 'undo_999'
#
# The following filename is different from the original datafile so this error is caught by InnoDB.
#
CREATE UNDO TABLESPACE undo_999 ADD DATAFILE 'MYSQL_UNDO_DIR/undo_003.ibu';
ERROR HY000: The ADD DATAFILE filepath already exists.
SHOW WARNINGS;
Level	Code	Message
Error	3121	The ADD DATAFILE filepath already exists.
Error	1528	Failed to create UNDO TABLESPACE undo_999
Error	3121	Incorrect File Name 'MYSQL_UNDO_DIR/undo_003.ibu'.
#
# The following filename is different from the original datafile so this error is caught by InnoDB.
#
CREATE UNDO TABLESPACE undo_999 ADD DATAFILE 'undo_004.ibu';
ERROR HY000: The ADD DATAFILE filepath already exists.
SHOW WARNINGS;
Level	Code	Message
Error	3121	The ADD DATAFILE filepath already exists.
Error	1528	Failed to create UNDO TABLESPACE undo_999
Error	3121	Incorrect File Name 'undo_004.ibu'.
#
# The following filename is exactly like the original datafile so this error is caught by MySQL.
#
CREATE UNDO TABLESPACE undo_999 ADD DATAFILE 'MYSQL_UNDO_DIR/undo_004.ibu';
ERROR HY000: Duplicate file name for tablespace 'undo_999'
SHOW WARNINGS;
Level	Code	Message
Error	3606	Duplicate file name for tablespace 'undo_999'
undo_001
undo_002
undo_003.ibu
undo_004.ibu
#
# Cleanup
#
# restart:
