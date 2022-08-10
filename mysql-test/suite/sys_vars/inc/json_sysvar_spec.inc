# ==== Purpose ====
#
# Define properties of system variables, in a JSON object.
#
# ==== Usage ====
#
# [--let $json_verbose = {0 | 1 | LIST}]
# --source json_sysvar_spec.inc
# --let $json_object = $sysvar_specs
# --source $json_sysvar_start
# while (!$json_sysvar_done) {
#   # Each iteration, process one sysvar.
#   # The sysvar is described with mtr variables, listed below.
#   --source $json_sysvar_next
# }
#
# # See e.g. all_global_sysvars_with_aliases.inc for a complete example.
#
# Parameters:
#
#   $json_verbose
#     Controls what output will be printed by $json_sysvar_next and
#     $json_sysvar_start.  See
#     include/create_unpacking_json_iterator.inc for a complete
#     description.
#
# When used as above, the loop will iterate over sysvars. In each iteration
# the following variables are set:
#
# $name
#   The name of the variable.
#
# $alias
#   If the variable has an alias, this should be set to the alias.
#   Otherwise an empty string.
#
# $default
#   The default value.
#
# $values
#   A JSON array containing at least three valid values.  The values
#   are chosen to be as "interesting" as possible to test, testing as
#   many syntax variations as possible. In addition:
#   - The first element is always different from the default.
#   - The Nth element is always different from the (N-1)th element.
#   - All elements are different, unless the range of the data type is
#     smaller than the length of the array.
#
# $invalid_values
#   A JSON array containing a set of invalid values.
#
# $min
#   The minimum value, if the variable is numeric. Otherwise an empty string.
#
# $max
#   The maximum value, if the variable is numeric. Otherwise an empty string.
#
# $block_size
#   The block size, if the variable is numeric. Otherwise 1.
#
# $global
#   Set to 1 for variables that exist in global scope, 0 for variables
#   that don't.
#
# $session
#   Set to 1 for variables that exist in session scope, 0 for
#   variables that don't.
#
# $cmdline
#   Set to 1 for variables that have a command-line counterpart, and 0
#   for variables that are not settable on the command-line.
#
# $persist
#   Set to 1 for persistable variables and 0 for non-persistable variables.
#
# $persist_as_readonly
#   Set to 1 for variables defined with the PERSIST_AS_READONLY flag,
#   and 0 for variables without it.  The PERSIST_AS_READONLY flag
#   controls how the variable is initialized during server start, if
#   it was found in the persisted variables file: without the flag,
#   the server exeuctes the equivalent of a SET statement. With the
#   flag, the server executes the equivalent of a command-line option.
#
# $dynamic
#   Set to 1 for dynamic variables and 0 for read-only variables.
#
# $value_map
#   Sometimes, the value you get when you use SELECT is not literally
#   identical to what you used in SET.  Examples are:
#   - For a string variable, you quote the value in SET, whereas
#     SELECT returns the value without quotes.
#   - For Boolean values, SET accepts 1, TRUE, ON, 'TRUE', etc,
#     whereas SELECT returns 0 or 1.
#   If any such values occur among $values or $default, your test
#   script may need to know what to expect from a SELECT statement.
#   It can find that value using this variable.  The format is a JSON
#   object, where the keys are values used in SET and the values are
#   the corresponding results observed in SELECT @@$name.
#
# $ps_value_map
#   Sometimes, the value shown in performance_schema is different from
#   the value returned by SELECT @@$name.  For example, Boolean
#   variables show the value 0 or 1 in SELECT @@$name, but OFF or ON
#   in performance_schema.  If any such values occur among $values,
#   your test may need to know what to expect when reading values from
#   performance_schema.  The format is a JSON object, where the keys
#   are values of SELECT @@$name, and the values are the corresponding
#   expected values in performance_schema.
#
# $skip_set_persist_only_default
#   Due to BUG#32761053, SET PERSIST_ONLY ... = DEFAULT is broken for
#   some variables, which makes it hard to test them.  For those
#   varibles, $skip_set_persist_only_default should be set to 1.
#
# $mask_value
#   If $values or $default contains nondeterministic values, for
#   example paths, $mask_value is set to 1.

# ==== Define "constants" that depend on environment ====

# Define numeric maxima.
--let $maxint32 = 4294967295
--let $maxint64 = 18446744073709551615
--source include/check_64bit_ulong.inc
if ($have_64bit_ulong) {
  --let $maxint = $maxint64
}
if (!$have_64bit_ulong) {
  --let $maxint = $maxint32
}

# Define numbers that depend on debug mode
--let $debug = `SELECT VERSION() LIKE '%debug%'`
if ($debug) {
  --let $if_debug_0_else_1 = 0
  --let $if_debug_1_else_8 = 1
  --let $if_debug_1_else_32 = 1
}
if (!$debug) {
  --let $if_debug_0_else_1 = 1
  --let $if_debug_1_else_8 = 8
  --let $if_debug_1_else_32 = 32
}

# Define paths
--let $tmpdir = `SELECT @@global.tmpdir`

# ==== Define abbreviations for common cases ====

# Define properties of booleans.
if(0){
  # Do not check values that begin with backtick, due to
  # BUG#32495164: MYSQLTEST DOUBLE-INTERPOLATES STRING VARIABLES
  #               STARTING WITH BACKTICK
  let $bool =
  "default": 0,
  "value_map": { "0": 0, "FALSE": 0, "OFF": 0, "'OFF'": 0, "`OFF`": 0, "1": 1, "TRUE": 1, "ON": 1, "'ON'": 1, "`ON`": 1 },
  "ps_value_map": {"0": "OFF", "1": "ON"},
  "values": ["ON", "OFF", 1, 0, "TRUE", "FALSE", "'ON'", "'OFF'", "`ON`", "`OFF`"],
  "invalid": [ 0.1, 2, -1, "''", "'x'", "'TRUE'", "'1'", "NULL"];
}
let $bool =
  "values": ["ON", "OFF", 1, 0, "TRUE", "FALSE", "'ON'", "'OFF'"],
  "invalid": [ 0.1, 2, -1, "''", "'x'", "'TRUE'", "'1'", "NULL"],
  "value_map": { "0": 0, "FALSE": 0, "OFF": 0, "'OFF'": 0, "1": 1, "TRUE": 1, "ON": 1, "'ON'": 1 },
  "ps_value_map": {"0": "OFF", "1": "ON"};

# Define invalid values for common data types.
--let $int_invalid = "invalid": [1.5, "ON", "'x'", "NULL"]
# Surprisingly, ON and NULL are allowed for strings, not listing them here.
--let $str_invalid = "invalid": [7, 1.9, "FALSE"]
--let $enum_invalid = 1.5, "ON", "'x'", "NULL", "'FOO_BAR'"

# Use this for variables that are read-only.
--let $readonly = "dynamic": 0

# Use this for variables that are non-persist.
--let $nopersist = "persist": 0

# Use this for variables that are persisted in "persist_as_readonly"
# mode.  (This has nothing to do with readonliness; it only means that
# the variable is loaded using the framework for command-line options
# rather than using the framework for SET statements.)
--let $persist_as_readonly = "persist_as_readonly": 1

# Use this for variables that are not settable on the command line.
--let $nocmdline = "cmdline": 0


# ==== Declare the array of sysvar descriptions ====
#
# This is a JSON document contains a JSON array where each element is
# a JSON object that describes one sysvar.  The JSON object has the
# following keys:
#
# - name: The name of the variable.
# - alias: The alias for the variable, if it has one.
# - default: The default value.
# - values: Array with at least three elements, containing some valid
#   values for the variable.
# - min: The minimum value, if it is a numeric variable.
# - max: The maximum value, if it is a numeric variable.
# - block_size: The "block size", if this is a numeric variable. It
#   defaults to 1 if not given.
# - invalid: Array of values that are not allowed to assign the
#   variable.
# - readonly: If given, and set to 1, the variable is expected to be
#   read-only.
# - nopersist: If given, and set to 1, the variable is expected to be
#   non-persistent.
# - value_map: Used when the value of a variable as seen by SELECT
#   @@variable differs from that in the preceding SET statement.  In
#   that case, this should be JSON object where the keys are values
#   that this script may use in a SET statement, and the values should
#   be the result of a subsequent SELECT.  Values that are not listed
#   in the object are assumed to be the same in SELECT as in the
#   preceding SET.  If value_map is omitted, it defults to the empty
#   set.
# - ps_value_map: Used when the value of a variable displayed in
#   performance_schema differs from the value displayed in a SELECT
#   @@variable statement. When this is the case, this should be a JSON
#   object where each key is a value shown by SELECT @@variable and
#   each value is the corresponding value shown in performance_schema.
#   Values that are not listed in the object are assumed to be the
#   same in SELECT @@variable and in performance_schema.  If
#   ps_value_map is omitted, it defaults to the empty set.

--delimiter |||
let $sysvar_specs =
[
  {
    "name": "init_replica",
    "alias": "init_slave",
    "default": "",
    "values": ["'SELECT 1'", "'SELECT 9'", "'SELECT(3)'"],
    "value_map": {"'SELECT 1'": "SELECT 1", "'SELECT 9'": "SELECT 9", "'SELECT(3)'": "SELECT(3)"},
    $str_invalid
  },
  {
    "name": "log_replica_updates",
    "alias": "log_slave_updates",
    $readonly,
    $persist_as_readonly,
    "default": 1,
    $bool
  },
  {
    "name": "log_slow_replica_statements",
    "alias": "log_slow_slave_statements",
    "default": 0,
    $bool
  },
  {
    "name": "replica_allow_batching",
    "alias": "slave_allow_batching",
    "default": 1,
    $bool
  },
  {
    "name": "replica_checkpoint_group",
    "alias": "slave_checkpoint_group",
    "default": 512,
    "values": [520, 1000, 2000],
    "min": $if_debug_1_else_32,
    "max": 524280,
    "block_size": $if_debug_1_else_8,
    $int_invalid
  },
  {
    "name": "replica_checkpoint_period",
    "alias": "slave_checkpoint_period",
    "default": 300,
    "values": [1, 2, 12323],
    "min": $if_debug_0_else_1,
    "max": $maxint32,
    $int_invalid
  },
  {
    "name": "replica_compressed_protocol",
    "alias": "slave_compressed_protocol",
    "default": 0,
    $bool
  },
  {
    "name": "replica_exec_mode",
    "alias": "slave_exec_mode",
    "default": "STRICT",
    "values": ["IDEMPOTENT", "STRICT", "'IDEMPOTENT'", 0, 1],
    "invalid": [$enum_invalid, -1, 2],
    "value_map": {"'IDEMPOTENT'": "IDEMPOTENT", "0": "STRICT", "1": "IDEMPOTENT"},
    "skip_set_persist_only_default": 1
  },
  {
    "name": "replica_load_tmpdir",
    "alias": "slave_load_tmpdir",
    $readonly,
    $nopersist,
    "default": "'$tmpdir'",
    "values": ["'$tmpdir/x'", "'$tmpdir'", "'$tmpdir/y'"],
    "invalid": ["NULL", "ON"],
    "value_map": {"'$tmpdir/x'": "$tmpdir/x", "'$tmpdir'": "$tmpdir", "'$tmpdir/y'": "$tmpdir/y"},
    "mask_value": 1
  },
  {
    "name": "replica_max_allowed_packet",
    "alias": "slave_max_allowed_packet",
    "default": 1073741824,
    "values": [1024, 2048, 65536],
    "min": 1024,
    "max": 1073741824,
    "block_size": 1024,
    $int_invalid
  },
  {
    "name": "replica_net_timeout",
    "alias": "slave_net_timeout",
    "default": 60,
    "values": [1, 2, 65536],
    "min": 1,
    "max": 31536000,
    $int_invalid
  },
  {
    "name": "replica_parallel_workers",
    "alias": "slave_parallel_workers",
    $persist_as_readonly,
    "default": 4,
    "values": [1, 2, 100],
    "max": 1024,
    $int_invalid
  },
  {
    "name": "replica_pending_jobs_size_max",
    "alias": "slave_pending_jobs_size_max",
    "default": 134217728,
    "values": [65536, 32768, 4096],
    "min": 1024,
    "max": 18446744073709550592,
    "block_size": 1024,
    $int_invalid
  },
  {
    "name": "replica_preserve_commit_order",
    "alias": "slave_preserve_commit_order",
    $persist_as_readonly,
    "default": 1,
    $bool
  },
  {
    "name": "replica_skip_errors",
    "alias": "slave_skip_errors",
    $readonly,
    $persist_as_readonly,
    "default": "OFF",
    "values": ["all", "ddl_exist_errors", "'1,2'", "'ddl_exist_errors,3,9,3,ddl_exist_errors,all'"],
    "invalid": [0, 1, 1.5, -1, "ON", null, "'x'"],
    "value_map": {"'1,2'": "1,2"},
    "skip_set_persist_only_default": 1
  },
  {
    "name": "replica_sql_verify_checksum",
    "alias": "slave_sql_verify_checksum",
    "default": 1,
    $bool
  },
  {
    "name": "replica_transaction_retries",
    "alias": "slave_transaction_retries",
    "default": 10,
    "values": [1, 2, 99],
    "min": 0,
    "max": $maxint,
    $int_invalid
  },
  {
    "name": "replica_type_conversions",
    "alias": "slave_type_conversions",
    "default": "",
    "values": ["ALL_LOSSY", "'ALL_NON_LOSSY'", "''", "'ALL_LOSSY,ALL_NON_LOSSY,ALL_UNSIGNED,ALL_SIGNED'", 0, 8, 15],
    "invalid": [$enum_invalid, -1, 16],
    "value_map": {
      "'ALL_NON_LOSSY'": "ALL_NON_LOSSY",
      "'ALL_LOSSY,ALL_NON_LOSSY,ALL_UNSIGNED,ALL_SIGNED'": "ALL_LOSSY,ALL_NON_LOSSY,ALL_UNSIGNED,ALL_SIGNED",
      "''": "",
      "0": "",
      "8": "ALL_SIGNED",
      "15": "ALL_LOSSY,ALL_NON_LOSSY,ALL_UNSIGNED,ALL_SIGNED"
    },
    "skip_set_persist_only_default": 1
  },
  {
    "name": "rpl_stop_replica_timeout",
    "alias": "rpl_stop_slave_timeout",
    "default": 31536000,
    "values": [100, 200, 3],
    "min": 2,
    "max": 31536000,
    $int_invalid
  },
  {
    "name": "source_verify_checksum",
    "alias": "master_verify_checksum",
    "default": 0,
    $bool
  },
  {
    "name": "sql_replica_skip_counter",
    "alias": "sql_slave_skip_counter",
    $nocmdline,
    "default": 0,
    "values": [1, 2, 12323],
    "min": 0,
    "max": $maxint32,
    $int_invalid
  },
  {
    "name": "sync_source_info",
    "alias": "sync_master_info",
    "default": 10000,
    "values": [1, 999, 123456],
    "min": 0,
    "max": $maxint32,
    $int_invalid
  }
]|||
--delimiter ;

# Removed the following due to
# BUG#32640588: SOURCE FOR SKIP_SLAVE_START DISPLAYED AS COMPILED WHEN IT IS PERSISTED
#  {
#    "name": "skip_replica_start",
#    "alias": "skip_slave_start",
#    $readonly,
#    $persist_as_readonly,
#    "default": 0,
#    $bool
#  },


# ==== Create iterator ====
#
# The JSON document declared above may be processed using the iterator
# declared below. See include/create_json_unpacking_iterator.inc for
# details.

--let $json_label = sysvar
--let $json_keys = name, alias, default, values, invalid, global, session, cmdline, persist, persist_as_readonly, dynamic, min, max, block_size, value_map, ps_value_map, skip_set_persist_only_default, mask_value
#--let $json_output_single_quoted = value_map, ps_value_map
--let $json_defaults = { "block_size": 1, "global": 1, "session": 0, "cmdline": 1, "persist": 1, "persist_as_readonly": 0, "dynamic": 1, "value_map": {}, "ps_value_map": {}, "skip_set_persist_only_default": 0, "mask_value": 0 }
--let $json_required = name, default, values, invalid

# Default and values are not included in $json_verbose, since they
# contain paths for some variables. Instead we just print them using
# an explicit 'echo' command in the loop, and an exception for
# variables containing a path.
# min, max, and block_size are not included in $json_verbose, since
# they differ between platforms or between debug and optimized build
# for some variables.
if (!$json_verbose) {
  --let $json_verbose = name, alias, invalid, min, max, block_size, global, session, cmdline, persist, persist_as_readonly, dynamic
}

--source include/create_json_unpacking_iterator.inc

# Sanity-check the JSON
--let $json = $sysvar_specs
--source include/json_check.inc
