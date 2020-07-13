#
# Call innodb_redo_log_archive_start() and -stop() with wrong parameters.
#
DO innodb_redo_log_archive_start();
ERROR HY000: Can't initialize function 'innodb_redo_log_archive_start'; Invalid number of arguments.
DO innodb_redo_log_archive_start(NULL);
ERROR HY000: Can't initialize function 'innodb_redo_log_archive_start'; First argument must not be null.
DO innodb_redo_log_archive_start(1);
ERROR HY000: Can't initialize function 'innodb_redo_log_archive_start'; Invalid first argument type.
DO innodb_redo_log_archive_start('', 1);
ERROR HY000: Can't initialize function 'innodb_redo_log_archive_start'; Invalid second argument type.
DO innodb_redo_log_archive_start('', '', '');
ERROR HY000: Can't initialize function 'innodb_redo_log_archive_start'; Invalid number of arguments.
DO innodb_redo_log_archive_stop(NULL);
ERROR HY000: Can't initialize function 'innodb_redo_log_archive_stop'; Invalid number of arguments.
DO innodb_redo_log_archive_stop(1);
ERROR HY000: Can't initialize function 'innodb_redo_log_archive_stop'; Invalid number of arguments.
DO innodb_redo_log_archive_stop('');
ERROR HY000: Can't initialize function 'innodb_redo_log_archive_stop'; Invalid number of arguments.

#
# Unprivileged user cannot start nor stop redo log archiving.
#
CREATE USER mysqltest_u1@localhost;
[connection mysqltest_u1]
DO innodb_redo_log_archive_start('label1');
ERROR 42000: Access denied; you need (at least one of) the INNODB_REDO_LOG_ARCHIVE privilege(s) for this operation
DO innodb_redo_log_archive_stop();
ERROR 42000: Access denied; you need (at least one of) the INNODB_REDO_LOG_ARCHIVE privilege(s) for this operation
[connection default]
DROP USER mysqltest_u1@localhost;

#
# NULL innodb_redo_log_archive_dirs.
#
SET @@global.innodb_redo_log_archive_dirs = NULL;
DO innodb_redo_log_archive_start('label1');
ERROR HY000: Server variable 'innodb_redo_log_archive_dirs' is NULL or empty

#
# Empty innodb_redo_log_archive_dirs.
#
SET @@global.innodb_redo_log_archive_dirs = '';
DO innodb_redo_log_archive_start('label1');
ERROR HY000: Server variable 'innodb_redo_log_archive_dirs' is NULL or empty

#
# Non-existen label.
#
SET @@global.innodb_redo_log_archive_dirs = 'label1:IRLA_DIRECTORY_1';
DO innodb_redo_log_archive_start('no-such-label');
ERROR HY000: Label 'no-such-label' not found in server variable 'innodb_redo_log_archive_dirs'

#
# Subdir with path separator.
#
SET @@global.innodb_redo_log_archive_dirs = 'label1:IRLA_DIRECTORY_1';
DO innodb_redo_log_archive_start('label1', 'tmp/tmp');
ERROR HY000: Redo log archiving start prohibits path name in 'subdir' argument

#
# Directory does not exist.
#
SET @@global.innodb_redo_log_archive_dirs = 'label1:IRLA_DIRECTORY_1';
DO innodb_redo_log_archive_start('label1');
ERROR HY000: Redo log archive directory 'IRLA_DIRECTORY_1' does not exist or is not a directory

#
# Directory is a file.
#
SET @@global.innodb_redo_log_archive_dirs = 'label1:IRLA_DIRECTORY_1';
DO innodb_redo_log_archive_start('label1');
ERROR HY000: Redo log archive directory 'IRLA_DIRECTORY_1' does not exist or is not a directory

#
# Directory clashes with server directories.
#
# Stop server
#
#
# Select unique path names for the server directories
#
#
# Precautionary cleanup of the new server directories
#
#
# Create the new server directories
#
#
# Move files from the default directories to the new server directories
#
#
# Restart the server with the new server directories
#
#
# Set innodb_redo_log_archive_dirs to the server directories
#


SET @@global.innodb_redo_log_archive_dirs = '1:MYSQLTEST_VARDIR/mysqld.1/data//xxx;2:MYSQLTEST_VARDIR/log/innodb_data_home_dir/xxx;3:MYSQLTEST_VARDIR/log/innodb_directories;4:MYSQLTEST_VARDIR/log/innodb_log_group_home_dir;5:MYSQLTEST_VARDIR/log/innodb_temp_tablespaces_dir;6:MYSQLTEST_VARDIR/log/tmp;7:MYSQLTEST_VARDIR/log/tmp/innodb_tmpdir/xxx;8:MYSQLTEST_VARDIR/log/innodb_undo_directory;9:SECURE_FILE_PRIV/xxx';
#
# Try to start redo log archiving with each of the server directories
#
DO innodb_redo_log_archive_start('1');
ERROR HY000: Redo log archive directory 'MYSQLTEST_VARDIR/mysqld.1/data//xxx' is in, under, or over server directory 'datadir' - 'MYSQLTEST_VARDIR/mysqld.1/data/'
DO innodb_redo_log_archive_start('2');
ERROR HY000: Redo log archive directory 'MYSQLTEST_VARDIR/log/innodb_data_home_dir/xxx' is in, under, or over server directory 'innodb_data_home_dir' - 'MYSQLTEST_VARDIR/log/innodb_data_home_dir/'
DO innodb_redo_log_archive_start('3');
ERROR HY000: Redo log archive directory 'MYSQLTEST_VARDIR/log/innodb_directories' is in, under, or over server directory 'innodb_directories' - 'MYSQLTEST_VARDIR/log/innodb_directories/'
DO innodb_redo_log_archive_start('4');
ERROR HY000: Redo log archive directory 'MYSQLTEST_VARDIR/log/innodb_log_group_home_dir' is in, under, or over server directory 'innodb_log_group_home_dir' - 'MYSQLTEST_VARDIR/log/innodb_log_group_home_dir/'
DO innodb_redo_log_archive_start('5');
ERROR HY000: Redo log archive directory 'MYSQLTEST_VARDIR/log/innodb_temp_tablespaces_dir' is in, under, or over server directory 'innodb_temp_tablespaces_dir' - 'MYSQLTEST_VARDIR/log/innodb_temp_tablespaces_dir/'
DO innodb_redo_log_archive_start('6');
ERROR HY000: Redo log archive directory 'MYSQLTEST_VARDIR/log/tmp' is in, under, or over server directory 'innodb_tmpdir' - 'MYSQLTEST_VARDIR/log/tmp/innodb_tmpdir/'
DO innodb_redo_log_archive_start('7');
ERROR HY000: Redo log archive directory 'MYSQLTEST_VARDIR/log/tmp/innodb_tmpdir/xxx' is in, under, or over server directory 'innodb_tmpdir' - 'MYSQLTEST_VARDIR/log/tmp/innodb_tmpdir/'
DO innodb_redo_log_archive_start('8');
ERROR HY000: Redo log archive directory 'MYSQLTEST_VARDIR/log/innodb_undo_directory' is in, under, or over server directory 'innodb_undo_directory' - 'MYSQLTEST_VARDIR/log/innodb_undo_directory/'
DO innodb_redo_log_archive_start('9');
ERROR HY000: Redo log archive directory 'SECURE_FILE_PRIV/xxx' is in, under, or over server directory 'secure_file_priv' - 'SECURE_FILE_PRIV'
#
# Stop server
#
#
# Move files back to the default directories
#
#
# Remove the new server directories
#
#
# Restart the server with default directories
#

#
# Set innodb_redo_log_archive_dirs for the following tests.
#
SET @@global.innodb_redo_log_archive_dirs = 'label1:IRLA_DIRECTORY_1;label2:IRLA_DIRECTORY_2';

#
# Create the redo log archive directory for the following tests.
#

#
# Create connection con1 for the following tests.
#

#
# Start with active logging.
#
DO innodb_redo_log_archive_start('label1');
DO innodb_redo_log_archive_start('label1');
ERROR HY000: Redo log archiving has been started on 'IRLA_PATHNAME_1' - Call innodb_redo_log_archive_stop() first
DO innodb_redo_log_archive_stop();

#
# Stop with inactive logging.
#
DO innodb_redo_log_archive_stop();
ERROR HY000: Redo log archiving is not active

#
# Start with active logging from other session.
#
DO innodb_redo_log_archive_start('label1');
[connection con1]
DO innodb_redo_log_archive_start('label2');
ERROR HY000: Redo log archiving has been started on 'IRLA_PATHNAME_1' - Call innodb_redo_log_archive_stop() first
[connection default]
DO innodb_redo_log_archive_stop();

#
# Stop from wrong session
#
DO innodb_redo_log_archive_start('label1');
[connection con1]
DO innodb_redo_log_archive_stop();
ERROR HY000: Redo log archiving has not been started by this session
[connection default]
DO innodb_redo_log_archive_stop();

#
# Session ends without stop
#
[connection con1]
DO innodb_redo_log_archive_start('label1');
# Redo log archive file exists
#
# Disconnect con1
#
[connection default]
# Redo log archive file does not exist any more

#
# Stop after error - reports the previous error
#
DO innodb_redo_log_archive_stop();
ERROR HY000: Redo log archiving failed: Session terminated with active redo log archiving - stopped redo log archiving and deleted the file.

#
# File exists
#
# Create a file with the name of the redo log archive file
#
DO innodb_redo_log_archive_start('label1');
ERROR HY000: Cannot create redo log archive file 'IRLA_PATHNAME_1' (OS errno: ## - ###)

#
# Call redo log archiving start & stop repeatedly
# This could catch race conditions between start and stop.
#
DO innodb_redo_log_archive_start("label1");
DO innodb_redo_log_archive_stop();
DO innodb_redo_log_archive_start("label1");
DO innodb_redo_log_archive_stop();
DO innodb_redo_log_archive_start("label1");
DO innodb_redo_log_archive_stop();
DO innodb_redo_log_archive_start("label1");
DO innodb_redo_log_archive_stop();
DO innodb_redo_log_archive_start("label1");
DO innodb_redo_log_archive_stop();
DO innodb_redo_log_archive_start("label1");
DO innodb_redo_log_archive_stop();
DO innodb_redo_log_archive_start("label1");
DO innodb_redo_log_archive_stop();
DO innodb_redo_log_archive_start("label1");
DO innodb_redo_log_archive_stop();
DO innodb_redo_log_archive_start("label1");
DO innodb_redo_log_archive_stop();
DO innodb_redo_log_archive_start("label1");
DO innodb_redo_log_archive_stop();
DO innodb_redo_log_archive_start("label1");
DO innodb_redo_log_archive_stop();
DO innodb_redo_log_archive_start("label1");
DO innodb_redo_log_archive_stop();
DO innodb_redo_log_archive_start("label1");
DO innodb_redo_log_archive_stop();
DO innodb_redo_log_archive_start("label1");
DO innodb_redo_log_archive_stop();
DO innodb_redo_log_archive_start("label1");
DO innodb_redo_log_archive_stop();
DO innodb_redo_log_archive_start("label1");
DO innodb_redo_log_archive_stop();
DO innodb_redo_log_archive_start("label1");
DO innodb_redo_log_archive_stop();
DO innodb_redo_log_archive_start("label1");
DO innodb_redo_log_archive_stop();
DO innodb_redo_log_archive_start("label1");
DO innodb_redo_log_archive_stop();
DO innodb_redo_log_archive_start("label1");
DO innodb_redo_log_archive_stop();

#
# Try to disable redo logging from other session.
#
DO innodb_redo_log_archive_start('label1');

ALTER INSTANCE DISABLE INNODB REDO_LOG;
ERROR HY000: Cannot perform operation as InnoDB is archiving redo log. Please retry after stopping redo archive by invoking innodb_redo_log_archive_stop()

SELECT * FROM performance_schema.global_status WHERE variable_name like 'Innodb_redo_log_enabled';
VARIABLE_NAME	VARIABLE_VALUE
Innodb_redo_log_enabled	ON

DO innodb_redo_log_archive_stop();

#
# Try to start redo archiving when Innodb  redo logging is diabled
#
ALTER INSTANCE DISABLE INNODB REDO_LOG;

SELECT * FROM performance_schema.global_status WHERE variable_name like 'Innodb_redo_log_enabled';
VARIABLE_NAME	VARIABLE_VALUE
Innodb_redo_log_enabled	OFF

DO innodb_redo_log_archive_start('label1');
ERROR HY000: Cannot perform operation as InnoDB redo logging is disabled. Please retry after enabling redo log with ALTER INSTANCE

ALTER INSTANCE ENABLE INNODB REDO_LOG;

SELECT * FROM performance_schema.global_status WHERE variable_name like 'Innodb_redo_log_enabled';
VARIABLE_NAME	VARIABLE_VALUE
Innodb_redo_log_enabled	ON

#
# Cleanup
#
SET @@global.innodb_redo_log_archive_dirs = '';
