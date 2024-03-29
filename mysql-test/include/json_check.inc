# ==== Purpose ====
#
# Check the syntax of a JSON document. Report a useful error message
# if the check fails. Do nothing if the check succeeds.
#
# This script parses the error message that the server gives, extracts
# the position from it, and prints a bit of context before and after
# that position.
#
# ==== Usage ====
#
# --let $json = JSON_DOCUMENT
# --source include/json_check.inc

--let $_cj_result_log = $ENABLE_RESULT_LOG
--let $_cj_query_log = $ENABLE_QUERY_LOG
--let $_cj_abort_on_error = $ENABLE_ABORT_ON_ERROR
--disable_result_log
--disable_query_log
--disable_abort_on_error

--let $_cj_json = escape('\,$json)
eval SELECT JSON_TYPE('$_cj_json');
if ($mysql_errno) {
  --let $_cj_message_noescape = `SHOW ERRORS`
  --echo $_cj_message_noescape
  --let $_cj_message = escape('\,$_cj_message_noescape)
  if (`SELECT '$_cj_message' LIKE '%Invalid JSON text in argument 1 to function json_type:%'`) {
    --let $_cj_position = `SELECT REGEXP_REPLACE('$_cj_message', '.*" at position ', '')`
    --let $_cj_position = `SELECT REPLACE('$_cj_position', '.', '')`
    --let $_cj_before_count = 60
    if ($_cj_position < 60) {
      --let $_cj_before_count = $_cj_position
    }
    --let $_cj_before_text = `SELECT SUBSTR('$_cj_json', $_cj_position + 1 - $_cj_before_count, $_cj_before_count)`
    --let $_cj_after_text = `SELECT SUBSTR('$_cj_json', $_cj_position + 1, 60)`
    --echo Bad JSON at position $_cj_position
    --echo Before-context: <$_cj_before_text>
    --echo After-context: <$_cj_after_text>
  }
  --die Invalid JSON.
}

if ($_cj_result_log) {
  --enable_result_log
}
if ($_cj_query_log) {
  --enable_query_log
}
if ($_cj_abort_on_error) {
  --enable_abort_on_error
}
