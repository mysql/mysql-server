# ==== Purpose ====
#
# Assert that the error log contains a given messages. The messages
# are given as a sequence of regular expressions. User must notice:
#   a) that all message types are inspected ERROR, INFO, WARNINGS,
#   b) the messages in error log must appear in the same sequence as
#       in given regular expressions,
#   c) listing error message here doesn't add the error to exception
#      list.
# The script inspects only the tail of the error log generated from
# the last invocation of `include/save_error_log_position.inc`,
# or from the last invocation of this script; whatever happened most
# recently.
#
# ==== Usage ====
#
# --source include/save_error_log_position.inc
# ... generate some errors...
# let $error_pattern = {
#     ERROR_1_REGEX
#     ERROR_2_REGEX
#     ...
#   | SAVEPOS };
# [--let $assert_dont_fail = 1]
# [--let $keep_old_error_log_position = 1]
# [--let $error_log_server_number = NUMBER]
# --source include/assert_error_log.inc
#
# Parameters:
#
#   $error_pattern
#
#     Normally this is a string containing a regular expression,
#     matching the expected error.
#
#     This can be a multi-line string: the first line matches the
#     first error, the second line matches the second error, and so
#     on.
#
#     The special value SAVEPOS is used by
#     save_error_log_position.inc It should not normally be used in
#     any other context.
#
#   $keep_old_error_log_position
#
#     Normally, this script saves the current error log position after
#     reading the error log, so that the next invocation of
#     assert_error_log.inc starts after the current position.
#
#     To keep the old position, set this variable to 1.
#
#   $assert_dont_fail
#
#     If set to 1, failure to match the regular expression does not
#     make the test fail immediately. The output will fail with result
#     mismatch but the test can complete.  This can be used during
#     test development and debugging, but must not be used in tests
#     that are pushed to a main branch.
#
#   $error_log_server_number
#
#     By default, this script operates on the error log for the server
#     of the current connection.  To use this script from a connection
#     on a different server, set this to the number of the server
#     whose log should be inspected.

# Get server number
--let $_ael_server_number = $error_log_server_number
if (!$_ael_server_number) {
  --let $_ael_server_number = `SELECT @@global.server_id`
}

--let $include_filename = assert_error_log_sequence.inc [server: $_ael_server_number, pattern: $error_pattern]
--source include/begin_include_file.inc

--source include/disable_binlog.inc

if (!$rpl_debug) {
  --disable_query_log
}


# Get error log filename
--let $_ael_error_log_filename_save = $error_log_filename
if (!$error_log_filename) {
  if ($error_log_server_number) {
    --let $error_log_filename = $MYSQLTEST_VARDIR/log/mysqld.$_ael_server_number.err
  }
  if (!$error_log_server_number) {
    --let $error_log_filename = `SELECT IF(@@global.log_error = 'stderr', CONCAT('$MYSQLTEST_VARDIR/log/mysqld.', $_ael_server_number, '.err'), @@global.log_error)`
  }
}

# Get line count to skip
if (!$_ael_positions) {
  --let $_ael_positions = {"1":-1,"2":-1,"3":-1,"4":-1,"5":-1,"6":-1,"7":-1,"8":-1,"9":-1,"10":-1}
}
--let $json_object = $_ael_positions
--let $json_key = $_ael_server_number
--source include/json_lookup.inc
--let $ignore_line_count = $json_value
if ($ignore_line_count == -1) {
  if ($error_pattern != SAVEPOS) {
    if ($error_pattern != SHOW) {
      --die !!!ERROR IN TEST: use save_error_log_position.inc before the first call to assert_error_log.inc. The error log may contain errors from earlier test runs.
    }
  }
  --let $ignore_line_count = 0
}

# Load error log from file into temporary table
--source include/load_error_log.inc

if ($rpl_debug) {
  SELECT * FROM mtr.error_log;
}

# Compute number of errors.
--let $_ael_error_count = `SELECT COUNT(*) FROM mtr.error_log`
if ($rpl_debug) {
  --echo # ignored $ignore_line_count lines from the error log
  --echo # found $_ael_error_count new lines after that position
}

# Update position
--expr $_ael_new_offset = $ignore_line_count + $_ael_error_count
if (!$keep_old_error_log_position) {
  --let $_ael_positions = `SELECT JSON_SET('$json_object', '$."$_ael_server_number"', $_ael_new_offset)`

  if ($rpl_debug) {
    --echo _ael_positions=<$_ael_positions>
  }
}

if ($error_pattern != SAVEPOS) {


  # Load expected error patterns from $error_pattern into temporary table.
  CREATE TEMPORARY TABLE mtr.error_patterns (id INT PRIMARY KEY AUTO_INCREMENT, pattern TEXT);

  # tmp_error_patterns contains all rows of $error_pattern, including comments and empty lines.
  CREATE TEMPORARY TABLE mtr.tmp_error_patterns (id INT PRIMARY KEY AUTO_INCREMENT, pattern TEXT);
  # Convert a multi-line string into a multi-row INSERT, by replacing
  # newlines by:
  #  '),('
  # Apply QUOTE before the newline-to-multirow conversion, so that
  # quotes within each message remain escaped but quotes that surround
  # each row remain unescaped.
  --let $_ael_pattern_escaped = escape(\',$error_pattern)
  --let $_ael_pattern_list = `SELECT REPLACE(QUOTE('$_ael_pattern_escaped'), '\n', "'),('")`
  eval INSERT INTO mtr.tmp_error_patterns(pattern) VALUES($_ael_pattern_list);
  # Filter out empty lines and comments
  INSERT INTO mtr.error_patterns(pattern) SELECT pattern FROM mtr.tmp_error_patterns WHERE pattern NOT REGEXP '^ *(#.*)?$' ORDER BY id;
  DROP TEMPORARY TABLE mtr.tmp_error_patterns;

  --let $_ael_pattern_count = `SELECT COUNT(*) FROM mtr.error_patterns`


    # Check all error-messages
  UPDATE mtr.error_log SET suspicious = 0;
  UPDATE mtr.error_log as l
         SET suspicious=(select count(*)>0  from mtr.error_patterns as p  where l.line rlike p.pattern);

  # Move filtered-in rows to a a new table in order to recompute the
  # autoincrement column so that it becomes consecutive and can be
  # joined with the pattern table.
  CREATE TEMPORARY TABLE mtr.filtered_error_log (id INT PRIMARY KEY AUTO_INCREMENT, message TEXT);
  INSERT INTO mtr.filtered_error_log(message) SELECT line FROM mtr.error_log WHERE suspicious = 1 ORDER BY id;
  if ($rpl_debug) {
    SELECT * FROM mtr.filtered_error_log;
  }

  --let $_ael_included_error_count = `SELECT COUNT(*) FROM mtr.filtered_error_log`

  # Check if the number of errors is as expected
  if ($_ael_included_error_count != $_ael_pattern_count) {
    --expr $_ael_excluded_error_count = $_ael_error_count - $_ael_included_error_count
    --echo Expected $_ael_pattern_count errors in the error log, but got $_ael_included_error_count.
    --echo We excluded the first $ignore_line_count lines. From the remaining $_ael_error_count lines (up to line $_ael_new_offset), we excluded $_ael_excluded_error_count lines that matched a suppression pattern.
    --let $_ael_failure = 1
  }

  # Check if the error texts match the patterns
  if ($_ael_included_error_count == $_ael_pattern_count) {
    --let $_ael_find_mismatch = FROM mtr.filtered_error_log e, mtr.error_patterns p WHERE e.id = p.id AND NOT e.message REGEXP p.pattern
    --let $_ael_mismatch_count = `SELECT COUNT(*) $_ael_find_mismatch`
    if ($_ael_mismatch_count) {
      --echo There were $_ael_mismatch_count error log lines which did not match the expected pattern:
      --let $_ael_offset = 0
      while ($_ael_offset < $_ael_mismatch_count) {
        --let $_ael_error = `SELECT e.message $_ael_find_mismatch LIMIT $_ael_offset, 1`
        --let $_ael_pattern = `SELECT p.pattern $_ael_find_mismatch LIMIT $_ael_offset, 1`
        --inc $_ael_offset
        --echo Mismatch $_ael_offset: error: <$_ael_error>
        --echo Mismatch $_ael_offset: pattern: <$_ael_pattern>
      }
      --let $_ael_failure = 1
    }
  }

  # Report error
  if ($_ael_failure) {
    --echo Error patterns:
    SELECT * FROM mtr.error_patterns;
    --echo Errors found in log on server $_ael_server_number:
    SELECT * FROM mtr.filtered_error_log;
    if (!$assert_dont_fail) {
      --die Errors did not match the expected list of patterns.
    }
  }

  DROP TEMPORARY TABLE mtr.error_patterns;

  if ($error_pattern == SHOW) {
    if (!$assert_error_log_printed_do_not_check_in_this_line) {
      --let $assert_error_log_printed_do_not_check_in_this_line = 1
      --echo DO_NOT_CHECK_IN_THIS_LINE: include/show_error_log.inc should only be used for debugging. Never check in a test that calls it on success.
    }
    --echo # There were $_ael_included_error_count new errors on server $_ael_server_number:
    if ($_ael_included_error_count) {
      SELECT * FROM mtr.filtered_error_log;
    }
  }

  DROP TEMPORARY TABLE mtr.filtered_error_log;
}

DROP TEMPORARY TABLE mtr.error_log;

--let $error_log_filename = $_ael_error_log_filename_save
--source include/restore_binlog.inc

--let $include_filename = assert_error_log_sequence.inc
--source include/end_include_file.inc
