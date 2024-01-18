# ==== Purpose ====
#
# Suppress one or more errors or warnings, telling check-testcase that
# they are expected.  This executes mtr.suppress_warning once on each
# server, and disables/re-enables binary logging.
#
# ==== Usage ====
#
# --let $messages =
#   message1
#   message2
#   ...
# ;
# [--let $suppress_on_current_connection = 1]
# [--let $suppress_silent = 1]
# [--let $suppress_mode = ADD_SUPPRESSIONS | IGNORE_GLOBAL_SUPPRESSIONS]
# --source include/suppress_messages.inc
#
# Parameters:
#
#   $messages
#     List of messages to suppress. Each message should be a regular
#     expression, and appear on a new line. Empty lines are allowed.
#     Because of a bug in mtr, the number of quote characters must be
#     even, or else mtr fails to parse the 'let' statement.  If the
#     message contains an odd number of quote characters - e.g.,
#     because of words like "don't" - replace the quote by a wildcard,
#     i.e., by a single dot character ('.').
#
#   $suppress_on_current_connection
#     By default, the script executes as follows:
#     - if rpl/init.inc has been sourced, adds the suppressions on every
#       node in the toplogy (on the server_N connection).
#     - otherwise, adds the suppressions only on the current connection.
#     If this parameter is set, it adds the suppression only on the current
#     connection, even if rpl_init has been sourced.
#
#   $suppress_mode
#     If $suppress_mode is not set, or if it is set to
#     ADD_SUPPRESSIONS, this script adds new suppressions.  If
#     $suppress_mode is set to IGNORE_GLOBAL_SUPPRESSIONS, this script
#     instead un-suppresses all messages that match the patterns, in
#     case they appear as global suppressions.  This is useful if a
#     test needs to use assert_error_log.inc to check for a warning
#     that is suppressed globally.
#
#   $suppress_silent
#     If $suppress_silent is set to 1 then the suppressions themselves
#     and the includes are not printed to the mtr output during the
#     execution of this script.

--let $include_filename = suppress_messages.inc
if ($suppress_silent)
{
  --let $saved_include_silent = $include_silent
  --let $include_silent= 1
}
--source include/begin_include_file.inc
if ($suppress_silent)
{
  --let $include_silent= $saved_include_silent
}

if (!$rpl_debug) {
  --disable_query_log
}

--let $_sm_suppress_mode = UNKNOWN
if ($suppress_mode == '') {
  --let $_sm_suppress_mode = ADD_SUPPRESSIONS
  --let $_sm_action = suppresses
}
if ($suppress_mode == ADD_SUPPRESSIONS) {
  --let $_sm_suppress_mode = ADD_SUPPRESSIONS
  --let $_sm_action = suppresses
}
if ($suppress_mode == IGNORE_GLOBAL_SUPPRESSIONS) {
  --let $_sm_suppress_mode = IGNORE_GLOBAL_SUPPRESSIONS
  --let $_sm_action = un-suppresses
}
if ($_sm_suppress_mode == UNKNOWN) {
  --echo !!!ERROR IN TEST: suppress_mode=<$suppress_mode>
  --die !!!ERROR IN TEST: $suppress_mode was set to an unknown value.
}

--let $_sm_server_number = 1
--let $_sm_last_server_number = 1
--let $_sm_change_connection = 0
if ($rpl_inited) {
  if (!$suppress_on_current_connection) {
    --let $_sm_server_number = 1
    --let $_sm_last_server_number = $rpl_server_count
    --let $_sm_change_connection = 1
  }
  if ($suppress_on_current_connection) {
    --let $_sm_server_number = `SELECT @@global.server_id`
    --let $_sm_last_server_number = $_sm_server_number
  }
}

--let $_sm_message_list = escape(\',$messages)
--let $_sm_message_list = `SELECT REPLACE('$_sm_message_list', '\r', '\n')`
--let $_sm_message_list = escape(\',$_sm_message_list)

while ($_sm_server_number <= $_sm_last_server_number) {
  if ($_sm_change_connection) {
    --let $rpl_connection_name = server_$_sm_server_number
    --source include/connection.inc
  }
  --source include/rpl/disable_binlog.inc

  --let $_sm_message_tail = $_sm_message_list
  while ($_sm_message_tail) {
    --let $_sm_message = `SELECT SUBSTRING_INDEX('$_sm_message_tail', '\n', 1)`

    # Skip empty lines.
    if ($_sm_message) {
      if (!$suppress_silent) {
        --echo # Connection $_sm_server_number $_sm_action message <$_sm_message>.
      }
      --let $_sm_escaped_message = escape(\',$_sm_message)
      if ($_sm_suppress_mode == ADD_SUPPRESSIONS) {
        eval CALL mtr.add_suppression('$_sm_escaped_message');
      }
      if ($_sm_suppress_mode == IGNORE_GLOBAL_SUPPRESSIONS) {
        eval INSERT INTO mtr.test_ignored_global_suppressions VALUES ('$_sm_escaped_message');
      }
    }

    --let $_sm_message_tail = `SELECT SUBSTRING('$_sm_message_tail', LENGTH('$_sm_escaped_message') + 2)`
    --let $_sm_message_tail = escape(\',$_sm_message_tail)
  }

  --source include/rpl/restore_binlog.inc

  --inc $_sm_server_number
}

--let $include_filename = suppress_messages.inc
--source include/end_include_file.inc
