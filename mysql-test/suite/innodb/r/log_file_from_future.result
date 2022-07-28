# Prepare schema used in the tests.
# Wait until next redo file is being created...
# Create connection which generates spam to the redo log.
CALL log_spammer();
CALL log_spammer();
SET GLOBAL DEBUG = '+d,syncpoint_log_before_file_marked_as_full';
SET DEBUG_SYNC = 'now WAIT_FOR reached_log_before_file_marked_as_full';
# Kill and restart
Pattern "Marked the current redo log file .* as incomplete" found
# Cleanup.
SET DEBUG_SYNC = 'RESET';
