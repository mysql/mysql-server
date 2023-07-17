# ==== Purpose ====
#
# Check that values of system status variables related to storing
# triggers in Table Cache (Table_open_cache_misses,
# Table_open_cache_trigger_hits, Table_open_cache_triggers_misses,
# Table_open_cache_triggers_overflows) have got expected increments
# since the last execution of this script, fail with debug info if
# they have not.
#
# ==== Usage ====
#
# --let $expected_inc = { "tc_misses_inc": VALUE1, "trg_hits_inc": VALUE2, "trg_misses_inc" : VALUE3, "trg_overflows_inc" : VALUE4 }
# --source include/assert_trigger_cache_increment.inc
#
# $expected_inc
#   The JSON object which specifies expected increments.
#
# At the end of the test, source include/destroy_json_functions.inc
# to remove all auxiliary .inc files created by this script.
#
# ==== Example ====
#
# # Run SELECT reading from table which was absent in Table Cache so far.
# SELECT count(*) FROM t1;
#
# --let $expected_inc = { "tc_misses_inc": 1, "trg_hits_inc": 0, "trg_misses_inc" : 0, "trg_overflows_inc" : 0 }
# --source include/assert_trigger_cache_increment.inc
#
# Resulting output:
# include/assert.inc [Expected table cache misses increment : 1]
# include/assert.inc [Expected table cache triggers hits increment : 0]
# include/assert.inc [Expected table cache triggers misses increment : 0]
# include/assert.inc [Expected table cache triggers overflows increment : 0]
#

# If missing, create helper scripts to iterate through below JSON array.
--let $has_json_functions = `SELECT INSTR('$json_function_files','sv_check_inc')`
if (!$has_json_functions) {
  --let $json_label = sv_check_inc
  --let $json_keys = paramname, psname, varname, atext
  --source include/create_json_unpacking_iterator.inc
}

# Array describing each status variable to be checked -- what is the name
# of the corresponding script parameter with expected increment, what
# is name of variable to remember its value for future executions, and
# what is assertion text to be used for this variable.

let $json_array = [
  {
    "paramname" : "tc_misses_inc",
    "psname": "Table_open_cache_misses",
    "varname": "tc_misses_count",
    "atext": "Expected table cache misses increment"
  },
  {
    "paramname" : "trg_hits_inc",
    "psname": "Table_open_cache_triggers_hits",
    "varname": "trg_hits_count",
    "atext": "Expected table cache triggers hits increment"
  },
  {
    "paramname" : "trg_misses_inc",
    "psname": "Table_open_cache_triggers_misses",
    "varname": "trg_misses_count",
    "atext": "Expected table cache triggers misses increment"
  },
  {
    "paramname" : "trg_overflows_inc",
    "psname": "Table_open_cache_triggers_overflows",
    "varname": "trg_overflows_count",
    "atext": "Expected table cache triggers overflows increment"
  }
];

# Iterate through the above array checking increment for each variable
# it describes.
--source $json_sv_check_inc_start

while (!$json_sv_check_inc_done) {
  --disable_query_log
  --eval SET @oldvalue = ifnull(@$varname, 0)
  --eval SELECT variable_value INTO @$varname FROM performance_schema.session_status WHERE variable_name ='$psname'
  --let $expinc = `SELECT JSON_EXTRACT('$expected_inc', '$.$paramname')`
  --enable_query_log
  --let $assert_text = $atext : $expinc
  --let $assert_cond = [SELECT @$varname - @oldvalue] = $expinc
  --source include/assert.inc

  --source $json_sv_check_inc_next
}
