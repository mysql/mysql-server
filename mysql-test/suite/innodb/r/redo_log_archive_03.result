#
# Prepare the server for redo log archiving.
#
SET @@global.innodb_redo_log_archive_dirs = 'label1:IRLA_DIRECTORY_1';

#
# Create consume event shall fail
#
SET @@session.debug = '+d,redo_log_archive_bad_alloc';
DO innodb_redo_log_archive_start('label1');
ERROR HY000: Memory allocation error: redo_log_archive_consume_event in function redo_log_archive_start.
SET @@session.debug = '-d,redo_log_archive_bad_alloc';

#
# Start timeout
#
SET @@global.debug = '+d,innodb_redo_log_archive_start_timeout';
SET @@session.debug = '+d,innodb_redo_log_archive_start_timeout';
DO innodb_redo_log_archive_start('label1');
ERROR HY000: Redo log archiving start timed out
SET @@session.debug = '-d,innodb_redo_log_archive_start_timeout';
SET @@global.debug = '-d,innodb_redo_log_archive_start_timeout';

#
# Cleanup
#
SET @@global.innodb_redo_log_archive_dirs = '';
