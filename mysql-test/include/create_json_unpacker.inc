# ==== Purpose ====
#
# Create a named 'unpacker', which is capable of unpacking JSON
# objects into a set of mysqltest variables.  For instance, it might
# unpack { "x": 1, "y": 2 } into the variable assignments $x=1, $y=2.
# A single unpacker can be used with multiple JSON objects.
#
# The unpacker consists of a .inc file, which unpacks a fixed set of
# named keys given when creating the unpacker, from an object
# given when invoking the unpacker.
#
# This script exposes the filename of the .inc file as an mtr variable.
#
# ==== Usage ====
#
# --let $json_label = NAME
# --let $json_keys = KEY1[, KEY2[, ...]]
# [--let $json_required = 0 | 1 | LIST]
# [--let $json_defaults = JSON_OBJECT]
# [--let $json_output_json_quoted = 0 | 1 | LIST]
# [--let $json_output_single_quoted = 0 | 1 | LIST]
# [--let $json_verbose = 0 | 1 | LIST]
# --source include/create_json_unpacker.inc
#
# --let $json_object = { "KEY1": VAL1[, "KEY2": VAL2 [, ...]] }
# --source $json_NAME_unpack
# # Now there are mysqltest variables named $KEY1, $KEY2, ...
# # holding the values VAL1, VAL2, ...
#
# At the end of the test, source include/destroy_json_functions.inc
# to remove all .inc files created by this script and the related
# include/create_json_* scripts.
#
# Parameters:
#
#   $json_label
#     Identifier used to distinguish this unpacker from other unpackers.
#
#   $json_keys
#     Comma-separated list of keys to unpack.
#     Since these are going to be used as names of mtr variables, they
#     must all be 7-bit ascii, alphanumeric strings.
#
#   $json_required
#     By default, the unpacker will accept that keys are missing, and
#     return a default value (see $json_defaults).
#     If all the keys are required, set this to 1.
#     If some of the keys are required, set this to a comma-separated
#     list containing those keys which are required.
#     When a required key is missing, the test will fail.
#
#   $json_defaults
#     By default, the unpacker will return an empty string if a key
#     is missing. To use another default, set $json_defaults to a
#     JSON object where keys from $json_keys are mapped to their
#     default values.
#
#   $json_output_json_quoted
#   $json_output_single_quoted
#     These options control the output format.  If
#     $json_output_json_quoted is used, JSON strings in the output
#     retain their double quotes; otherwise the double quotes are
#     stripped using JSON_UNQUOTE.  If $json_output_single_quoted is
#     used, any backslash and single quote characters are escaped so
#     that the result can be used in a single-quoted string.
#     Examples:
#     json_quoted single_quoted   "blowin' in the wind"
#          0            0         blowin' in the wind
#          0            1         blowin\' in the wind
#          1            0         "blowin' in the wind"
#          1            1         "blowin\' in the wind"
#     To apply the same quoting rules to all unpacked values, set the
#     options to 0 or 1. To specify individual quoting for each key,
#     set the options to comma-separated lists of keys: the quoting
#     rule will be enabled for the values of the listed keys and
#     disabled for the values of keys that are not in the lists.
#
#   $json_verbose
#     By default, this script prints nothing. If this option is set to
#     1, the script prints all key-value pairs to the result log. If
#     this option is set to a comma-separated list of keys, the script
#     prints the key-value pair for the keys in the list.
#
# Output:
#
#   This script creates a .inc file in $MYSQLTEST_VARDIR/tmp which is
#   capable of unpacking values from a JSON document into mtr
#   variables.  If $json_label is NAME, this script sets the variable
#   $json_NAME_unpack to the filename of the .inc file.  The script
#   should be used as follows:
#
#     --let $json_object = JSON_OBJECT
#     [--let $json_allow_extra_keys = 1]
#     --source $json_NAME_unpack
#
#       Parameters:
#
#         $json_object
#
#           The object from which values will be unpacked.
#
#         $json_allow_extra_keys
#
#           By default, this script checks if $json_object contains
#           any keys that were not specified in $json_keys when
#           create_json_iterator was invoked. To suppress that check,
#           set $json_allow_extra_keys=1.
#
# ==== Examples ===
#
# See mysql-test/t/mysqltest_json.test.

# Get number of array elements and number of keys.
--let CJU_LABEL = $json_label
--let CJU_KEYS = $json_keys
--let CJU_DEFAULTS = $json_defaults
--let CJU_REQUIRED = $json_required
--let CJU_JSON_QUOTED = $json_output_json_quoted
--let CJU_SINGLE_QUOTED = $json_output_single_quoted
--let CJU_VERBOSE = $json_verbose
--let CJU_UUID = `SELECT UUID()`

perl END_OF_PERL;
  my $label = $ENV{'CJU_LABEL'};
  my $key_text = $ENV{'CJU_KEYS'};
  my $defaults_text = $ENV{'CJU_DEFAULTS'};
  my $required_text = $ENV{'CJU_REQUIRED'};
  my $json_quoted_text = $ENV{'CJU_JSON_QUOTED'};
  my $single_quoted_text = $ENV{'CJU_SINGLE_QUOTED'};
  my $verbose_text = $ENV{'CJU_VERBOSE'};
  my $uuid = $ENV{'CJU_UUID'};
  my $file_infix = "/tmp/json_$label";
  my $file_prefix = $ENV{'MYSQLTEST_VARDIR'} . $file_infix;
  my $file_prefix_generator = '$MYSQLTEST_VARDIR' . $file_infix;
  my $init_file = "${file_prefix}_init_unpacker.inc";
  my $unpack_file = "${file_prefix}_unpack.inc";
  my $var_prefix = 'json_' . $label;
  my $unpack_file_var = $var_prefix . '_unpack';
  my $defaults_var = $var_prefix . '_defaults';
  my $added_var = $var_prefix . '_added_unpacker_to_json_function_files';

  if ($label !~ /^[a-zA-Z0-9_]+$/) {
    die "!!!ERROR IN TEST: Set \$json_label an alphanumeric 7-bit ASCII identifier (it was '$label')";
  }
  if (!$key_text) {
    die "!!!ERROR IN TEST: Set \$json_keys.";
  }

  my @key_list = split / *, */, $key_text;
  my %key_hash = ();
  my $key_check_sql = '';
  for my $key (@key_list) {
    $key_hash{$key} = 1;
    $key_check_sql .= ", '\$.\"$key\"'";
  }

  my %required_hash = ();
  if ($required_text eq '1') { $required_text = $key_text; }
  elsif ($required_text eq '0') { $required_text = ''; }
  for my $key (split / *, */, $required_text) {
    if (!defined($key_hash{$key})) {
      die "!!!ERROR IN TEST: \$json_required contains key <$key> which is not in \$json_keys";
    }
    $required_hash{$key} = 1;
  }

  my %json_quoted_hash = ();
  if ($json_quoted_text eq '1') { $json_quoted_text = $key_text; }
  elsif ($json_quoted_text eq '0') { $json_quoted_text = ''; }
  for my $key (split(/ *, */, $json_quoted_text)) {
    if (!defined($key_hash{$key})) {
      die "!!!ERROR IN TEST: \$json_output_json_quoted contains key <$key> which is not in \$json_keys";
    }
    $json_quoted_hash{$key} = 1;
  }

  my %single_quoted_hash = ();
  if ($single_quoted_text eq '1') { $single_quoted_text = $key_text; }
  if ($single_quoted_text eq '0') { $single_quoted_text = ''; }
  for my $key (split(/ *, */, $single_quoted_text)) {
    if (!defined($key_hash{$key})) {
      die "!!!ERROR IN TEST: \$json_output_single_quoted contains key <$key> which is not in \$json_keys";
    }
    $single_quoted_hash{$key} = 1;
  }

  my %verbose_hash = ();
  if ($verbose_text eq '1') { $verbose_text = $key_text; }
  if ($verbose_text eq '0') { $verbose_text = ''; }
  for my $key (split(/ *, */, $verbose_text)) {
    if (!defined($key_hash{$key})) {
      die "!!!ERROR IN TEST: \$json_verbose contains key <$key> which is not in \$json_keys";
    }
    $verbose_hash{$key} = 1;
  }

  # INIT_FILE will be sourced at the end of create_json_unpacker.inc
  open(INIT_FILE, "> $init_file")
    or die "Error $? opening $init_file: $!";
  print INIT_FILE (###############################################
    "
    --let \$$unpack_file_var = ${file_prefix_generator}_unpack.inc
    --let \$$defaults_var = escape('\\,\$json_defaults)
    if (\$rpl_debug) {
      --echo # $unpack_file_var: $defaults_var=<\$$defaults_var>
      # This gives a useful error message for malformed JSON.
      if (\$$defaults_var) {
        eval SELECT JSON_TYPE('\$$defaults_var') AS JSON_TYPE;
      }
    }
    if (!\$$added_var) {
      --let \$$added_var = 1
      --let \$json_function_files = \$json_function_files ${file_prefix_generator}_init_unpacker.inc \$$unpack_file_var
    }

  "###############################################################
  ) or die "Error $? writing to $init_file: $!";

  close(INIT_FILE)
    or die "Error $? closing $init_file: $!";

  open(UNPACK_FILE, "> $unpack_file")
    or die "Error $? opening $unpack_file: $!";
  print UNPACK_FILE (############################################
    "
    # Check JSON syntax and get a reasonable error message.
    --let \$json = \$json_object
    --source include/json_check.inc

    --let \$json_object_escaped = escape('\\,\$json_object)
    if (\$rpl_debug) {
      --echo # $unpack_file_var: json_object=<\$json_object>
      --echo # $unpack_file_var: json_object_escaped=<\$json_object_escaped>
    }

    # Check that no keys other than the expected ones exist in the object.
    if (!\$json_allow_extra_keys) {
      if (`SELECT JSON_REMOVE('\$json_object_escaped'$key_check_sql) != CAST('{}' AS JSON)`) {
        --enable_result_log
        --echo json_object=<\$json_object>
        --echo json_keys=<\$json_keys>
        eval SELECT JSON_REMOVE('\$json_object_escaped'$key_check_sql) AS unknown_keys;
        --die !!!ERROR IN TEST: Unknown keys in json_object
      }
    }
    "###########################################################
  ) or die "Error $? writing to $unpack_file: $!";
  if (%verbose_hash) {
    print UNPACK_FILE (
      "--echo $label:\n"
    );
  }
  for my $key (split(/ *, */, $key_text)) {
    if ($key !~ /^[a-zA-Z0-9_]+$/) {
      die "!!!ERROR IN TEST: \$json_keys should contain only alphanumeric 7-bit ASCII identifiers";
    }
    my $o = "'\$json_object_escaped'";
    my $k = "'\$.\"$key\"'";
    my $func = 'JSON_EXTRACT';
    my $end = '';
    if (!defined($json_quoted_hash{$key})) {
      $func = "JSON_UNQUOTE($func";
      $end = ')';
    }
    if (defined($required_hash{$key})) {
      print UNPACK_FILE (#######################################
        "
        --let \$$key = `SELECT IF(JSON_CONTAINS_PATH($o, 'all', $k), $func($o, $k)$end, '$uuid')`
        if (\$$key == '$uuid') {
          --echo Missing value for \"$key\" in <\$json_object>
          --die Missing value for $key in json_object
        }
        "#######################################################
      ) or die "Error $? writing to $unpack_file: $!";
    } elsif ($defaults_text) {
      print UNPACK_FILE (
        "--let \$$key = `SELECT $func(IF(JSON_CONTAINS_PATH($o, 'all', $k), $o, '\$$defaults_var'), $k)$end`\n"
      ) or die "Error $? writing to $unpack_file: $!";
    } else {
      print UNPACK_FILE (
        "--let \$$key = `SELECT $func($o, $k)$end`\n"
      );
    }
    if (defined($single_quoted_hash{$key})) {
      print UNPACK_FILE (
        "
        --let \$$key = escape('\\,\$$key)
        "
      );
    }
    if (defined($verbose_hash{$key})) {
      print UNPACK_FILE (
        "--echo - $key=\$$key\n"
      ) or die "Error $? writing to $unpack_file: $!";
    }
  }
  close(UNPACK_FILE)
    or die "Error $? closing $unpack_file: $!";
END_OF_PERL

--let $cju_init = _init_unpacker
--source $MYSQLTEST_VARDIR/tmp/json_$json_label$cju_init.inc
