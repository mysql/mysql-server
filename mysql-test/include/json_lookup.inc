# ==== Purpose ====
#
# Lookup the value of an mtr variable in a JSON object and return the result.
#
# ==== Usage ====
#
# --let $json_key = VALUE
# --let $json_object = JSON_OBJECT
# [--let $json_lookup_output_single_quote_escaped]
# --source include/json_lookup.inc
# # now $json_value contains the result.
#
# $json_key
#   The string value for the key that will be looked up.
#
# $json_object
#   The JSON object in which this script will lookup $json_key.
#   If this is an empty string, or if it is a JSON object where $json_key
#   does not appear as a key, $json_value will be set to $json_key.
#
# $json_value
#   The output value.
#
# $json_lookup_output_single_quote_escaped
#   By default, the result will contain the literal value stored in the
#   JSON object, with double quotes removed using JSON_UNQUOTE.
#   If this parameter is set, the result will have backslashes and single
#   quotes escaped.
#
# ==== Example ====
#
# --let $json_object = { "Chuck Berry": "rock 'n' roll", "Miles Davis": "jazz" }
# --let $json_key = Chuck Berry
#
# --source include/json_lookup.inc
# # Prints "Chuck Berry played rock 'n' roll."
# --echo $json_key played $json_value.
#
# --let $json_lookup_output_single_quoted = 1
# --source include/json_lookup.inc
# # Returns the string "Chuck Berry played rock \'n\' roll."
# eval SELECT '$json_key played $json_value.';

if ($json_object != '') {
  --let $json = $json_object
  --source include/json_check.inc

  --let $_jm_key = escape('\,$json_key)
  --let $_jm_key_double_quotes_escaped = escape(",$_jm_key)
  --let $_jm_object = escape('\,$json_object)
  if ($rpl_debug) {
    --echo DEBUG: json_lookup: json_key=<$json_key>
    --echo DEBUG: json_lookup: _jm_object=<$_jm_object>
    --echo DEBUG: json_lookup: _jm_key=<$_jm_key>
    --echo DEBUG: json_lookup: _jm_key_double_quotes_escaped=<$_jm_key_double_quotes_escaped>
  }
  --let $json_value = `SELECT IF(JSON_CONTAINS_PATH('$_jm_object', 'one', '$."$_jm_key_double_quotes_escaped"'), JSON_UNQUOTE(JSON_EXTRACT('$_jm_object', '$."$_jm_key_double_quotes_escaped"')), '$_jm_key')`
  if ($rpl_debug) {
    --echo DEBUG: json_lookup: json_value=<$json_value>
  }
}

if ($json_object == '') {
  --let $json_value = $json_key
  if ($rpl_debug) {
    --echo DEBUG: json_lookup: json_value=json_key=<$json_value>
  }
}

if ($json_lookup_output_single_quote_escaped) {
  --let $json_value = escape('\,$json_value)
}
if ($rpl_debug) {
  --echo DEBUG: json_lookup: returning json_value=<$json_value>
}
