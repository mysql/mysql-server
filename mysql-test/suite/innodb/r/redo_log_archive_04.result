#
# Show the original variable value (from -master.opt).
#
SELECT @@global.innodb_redo_log_archive_dirs;
@@global.innodb_redo_log_archive_dirs
command_line_label:/

#
# Try to start redo log archiving with that value.
#
DO innodb_redo_log_archive_start('command_line_label');
Got one of the listed errors

#
# Set innodb_redo_log_archive_dirs for the following tests.
#
SET @@global.innodb_redo_log_archive_dirs = 'label1:IRLA_DIRECTORY_1';

#
# Directory with world permissions
#
DO innodb_redo_log_archive_start('label1');
ERROR HY000: Redo log archive directory 'IRLA_DIRECTORY_1' is accessible to all OS users

#
# Redo log archive file is read-only. No world permissions
#
DO innodb_redo_log_archive_start('label1');
#
# Check the redo log archive file for existence and permissions.
#
include/assert.inc [File permissions should be equal to 440]
#
# Stop redo log archiving.
#
DO innodb_redo_log_archive_stop();
#
# Check the redo log archive file again.
#
include/assert.inc [File permissions should be equal to 440]
#
# Remove the redo log archive file.
#

#
# Cleanup
#
SET @@global.innodb_redo_log_archive_dirs = 'command_line_label:/';
