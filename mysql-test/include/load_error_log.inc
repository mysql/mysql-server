# ==== Purpose ====
#
# Create a temporary table on the current session, and fill it with
# lines from the given error log file.
#
# This is used by the check-warnings script that runs after each test.
#
# ==== Usage ====
#
# --let $error_log_filename = FILENAME
# --let $ignore_line_count = N
# --source include/load_error_log.inc
#
# Parameters:
#
#  $error_log_filename
#    The error log file to read.
#
#  $ignore_line_count
#    Skip this many lines from the file.

--let $include_filename = load_error_log.inc
--source include/begin_include_file.inc

CREATE TEMPORARY TABLE mtr.error_log (
  id INT AUTO_INCREMENT PRIMARY KEY,
  suspicious INT DEFAULT 1,
  line VARCHAR(10000) CHARSET latin1 DEFAULT NULL
);

COMMIT;

# Some tests generate very wide error log lines (e.g., 16k for
# innodb.innodb_bug14147491).  That does not fit in the 'line' column
# of the table, so it generates errors like "Data truncated for column
# 'line' at row 244".  We demote that to a warning by disabling
# strictness in sql_mode.  We also suppress printing warnings to the
# result file, so we don't fail with result mismatch.
--let $_lel_sql_mode_save = `SELECT @@session.sql_mode`
SET @@session.sql_mode = '';
--disable_warnings

# Try to load the warnings into a temporary table, it might fail with
# error saying "The MySQL server is running with the
# --secure-file-priv" in which case an attempt to load the file using
# LOAD DATA LOCAL is made.
--error 0,1290
eval LOAD DATA INFILE '$error_log_filename' INTO TABLE mtr.error_log
  CHARACTER SET latin1
  FIELDS TERMINATED BY 'xykls37' ESCAPED BY ''
  IGNORE $ignore_line_count LINES
  (line)
  ;


if ($mysql_errno) {
  # Try LOAD DATA LOCAL
  eval LOAD DATA LOCAL INFILE '$error_log_filename' INTO TABLE mtr.error_log
    CHARACTER SET latin1
    FIELDS TERMINATED BY 'xykls37' ESCAPED BY ''
    IGNORE $ignore_line_count LINES
    (line);
}

eval SET @@session.sql_mode = '$_lel_sql_mode_save';

--source include/end_include_file.inc
