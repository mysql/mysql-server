#
# Prepare the server for redo log archiving.
#
SET @@global.innodb_redo_log_archive_dirs = 'label1:IRLA_DIRECTORY_1;label2:IRLA_DIRECTORY_2;:IRLA_DIRECTORY_3;';

#
# Start and stop redo log archiving. Using SELECT.
#
SELECT innodb_redo_log_archive_start('label1');
innodb_redo_log_archive_start('label1')
0
SELECT innodb_redo_log_archive_stop();
innodb_redo_log_archive_stop()
0
#
# Remove the redo log archive file.
#

#
# Start and stop redo log archiving. Using DO. Result is constant anyway.
#
DO innodb_redo_log_archive_start('label1');
DO innodb_redo_log_archive_stop();
#
# Remove the redo log archive file.
#

#
# Start and stop redo log archiving. With NULL subdir.
#
DO innodb_redo_log_archive_start('label1', NULL);
DO innodb_redo_log_archive_stop();
#
# Remove the redo log archive file.
#

#
# Start and stop redo log archiving. With empty subdir.
#
DO innodb_redo_log_archive_start('label1', '');
DO innodb_redo_log_archive_stop();
#
# Remove the redo log archive file.
#

#
# Start and stop redo log archiving. With subdir.
#
DO innodb_redo_log_archive_start('label2', 'subdir_2');
DO innodb_redo_log_archive_stop();
#
# Remove the redo log archive file.
#

#
# Start redo log archiving. With empty label
#
DO innodb_redo_log_archive_start('', 'subdir_3');

#
# Create a lot of InnoDB redo log.
#
DROP DATABASE IF EXISTS db1;
CREATE DATABASE db1;
CREATE TABLE db1.t1(id int, b longblob) ENGINE=InnoDB;
INSERT INTO db1.t1 VALUES(1, repeat(1, 22*1024*1024));
DELETE FROM db1.t1;
DROP TABLE db1.t1;
DROP DATABASE db1;

#
# Stop redo log archiving.
#
DO innodb_redo_log_archive_stop();

#
# Check the minimum size of the redo log archive file.
#

#
# Remove the redo log archive file.
#

#
# Privileged user can start and stop redo log archiving.
#
CREATE USER mysqltest_u1@localhost;
GRANT INNODB_REDO_LOG_ARCHIVE ON *.* TO mysqltest_u1@localhost;
[connection u1]
DO innodb_redo_log_archive_start('label1');
DO innodb_redo_log_archive_stop();
#
# Remove the redo log archive file.
#
[connection default]

#
# Restart server
#
SET @@global.innodb_redo_log_archive_dirs = 'label1:IRLA_DIRECTORY_1';
#
# Verify that the user does still have the INNODB_REDO_LOG_ARCHIVE privilege.
#
SHOW GRANTS FOR mysqltest_u1@localhost;
Grants for mysqltest_u1@localhost
GRANT USAGE ON *.* TO `mysqltest_u1`@`localhost`
GRANT INNODB_REDO_LOG_ARCHIVE ON *.* TO `mysqltest_u1`@`localhost`
#
# Verify that the user can still start and stop redo log archiving.
#
[connection u1]
DO innodb_redo_log_archive_start('label1');
DO innodb_redo_log_archive_stop();
#
# Remove the redo log archive file.
#
[connection default]
DROP USER mysqltest_u1@localhost;

#
# Cleanup
#
SET @@global.innodb_redo_log_archive_dirs = '';
