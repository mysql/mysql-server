# ==== Purpose ====
#
# Create a named 'iterator' over JSON arrays.
#
# The iterator consists of two .inc files: one to start iteration, and
# one to step forward.  The iterator is bound to a JSON array when
# sourcing the start file.  This file exposes the two filenames
# through two mysqltest variables.
#
# Multiple iterators may be used in nested loops. Each iterator has a
# "label" which makes it possible to distinguish the different
# iterators.  The same iterator may be started multiple times with
# different objects; this makes it possible to reuse the iterator of
# the inner loop for each iteration of the outer loop, so the iterator
# does not have be rebuilt.
#
# ==== Usage ====
#
# --let $json_label = NAME
# [--let $json_unquote = 0]
# --source include/create_json_iterator.inc
# # Now:
# # - To start iteration, set $json_array to the JSON array which you need
# #   to iterate over, and then source the file $json_NAME_start.
# # - To check when iteration is complete, check $json_NAME_done.
# # - To inspect the current value, inspect $json_NAME_value.
# # - To jump to next value, source $json_NAME_next.
#
# At the end of the test, source include/destroy_json_functions.inc
# to remove all .inc files created by this script and the related
# include/create_json_* scripts.
#
# Parameters:
#
#   $json_label
#     Identifier used to distinguish this iterator from other iterators.
#     This should use singular and describe what goes in one element of
#     the array.
#
#   $json_unquote
#     By default, the JSON_UNQUOTE function is used to remove quotes
#     around returned JSON strings.  To disable that, set
#     $json_unquote=0.
#
# Output:
#
#   This script generates two .inc files in $MYSQLTEST_VARDIR/tmp,
#   which can be used to iterate over a JSON array.  If $json_label is
#   NAME, the script names are stored in the mysqltest variables
#   $json_NAME_start and $json_NAME_next. These scripts should be used
#   as follows:
#
#   --let $json_array = JSON_ARRAY
#   [--let $json_start = NUMBER]
#   [--let $json_stride = NUMBER]
#   [--let $rpl_debug = 1]
#   --source $json_NAME_start
#
#     Start the iteration. By default, it sets $json_NAME_done to 1 if
#     the array is empty; otherwise it sets $json_NAME_value to the
#     first element of the array, and sets $json_NAME_index to the
#     next element index, i.e., 1.  The start element and step length can
#     be controlled with the parameters.
#
#     Parameters:
#
#       $json_start
#         Start from the given array index, rather than 0.
#
#       $json_stride
#         Move this number of steps, rather than 1 forwards, in the
#         following iterations.  This can be any number other than 0.
#         If $json_start is not given, iteration will begin at the first
#         element (index 0) if $json_stride > 0, and at the last element
#         if $json_stride < 0.  (Internally, this sets the start value
#         for $json_NAME_stride, which you may later alter before each
#         invocation of $json_NAME_next.)
#
#       $rpl_debug
#         Print debug information.
#
#   [--let $json_NAME_index = NUMBER]
#   [--let $json_NAME_stride = NUMBER]
#   [--let $rpl_debug = 1]
#   --source $json_NAME_next
#
#     Move to the next iteration.  If it reaches the end of the array,
#     it sets $json_NAME_done to 1. Otherwise it sets $json_NAME_value
#     to the value of the next element.
#
#     Parameters:
#
#       $json_NAME_index
#         The array index that will be inspected.  This is
#         automatically initialized by $json_NAME_start and updated by
#         $json_NAME_index, so you do not need to alter it unless you
#         need to take a different step length.
#
#       $json_NAME_stride
#         The step length that will be added to $json_NAME_index.
#         This is automatically initialized by $json_NAME_start, so
#         you only need to alter it if you need to change the step
#         length during the iteration.
#
#      $rpl_debug
#         Print debug information.
#
# ==== Examples ====
#
# See mysql-test/t/mysqltest_json.test.


--let CJI_LABEL = $json_label
--let CJI_UNQUOTE = $json_unquote

# $json_function_files tracks the list of all files we create, so that
# destroy_json_functions.inc can remove them.
--let CJI_JSON_FUNCTION_FILES = $json_function_files

perl END_OF_PERL;
  my $label = $ENV{'CJI_LABEL'};
  my $unquote = $ENV{'CJI_UNQUOTE'};
  my $function_files = $ENV{'CJI_JSON_FUNCTION_FILES'};
  my $file_infix = "/tmp/json_$label";
  my $file_prefix = $ENV{'MYSQLTEST_VARDIR'} . $file_infix;
  my $file_prefix_generator = '$MYSQLTEST_VARDIR' . $file_infix;
  my $init_file = "${file_prefix}_init_iterator.inc";
  my $start_file = "${file_prefix}_start.inc";
  my $next_file = "${file_prefix}_next.inc";
  my $var_prefix = 'json_' . $label;
  my $array_var = $var_prefix . '_array';
  my $index_var = $var_prefix . '_index';
  my $done_var = $var_prefix . '_done';
  my $length_var = $var_prefix . '_length';
  my $value_var = $var_prefix . '_value';
  my $start_file_var = $var_prefix . '_start';
  my $next_file_var = $var_prefix . '_next';
  my $stride_var = $var_prefix . '_stride';
  my $added_var = $var_prefix . '_added_iterator_to_json_function_files';

  if ($label !~ /^[a-zA-Z0-9_]+$/) {
    die "!!!ERROR IN TEST: Set \$json_label an alphanumeric 7-bit ASCII identifier (it was '$label')";
  }

  open(INIT_FILE, "> $init_file")
    or die "Error $? opening $init_file: $!";
  print INIT_FILE ("
    --let \$$start_file_var = ${file_prefix_generator}_start.inc
    --let \$$next_file_var = ${file_prefix_generator}_next.inc
    if (!\$$added_var) {
      --let \$$added_var = 1
      --let \$json_function_files = \$json_function_files ${file_prefix_generator}_init_iterator.inc \$$start_file_var \$$next_file_var
    }
  ") or die "Error $? writing to $init_file: $!";

  close(INIT_FILE)
    or die "Error $? closing $init_file: $!";

  open(START_FILE, "> $start_file")
    or die "Error $? opening $start_file: $!";
  print START_FILE (#################################
    "
    if (!\$json_array) {
      --die !!!ERROR IN TEST: set \$json_array first
    }
    # Check json syntax and get a reasonable error message
    --let \$json = \$json_array
    --source include/json_check.inc
    --let \$$array_var = escape('\\,\$json_array)
    --let \$$length_var = `SELECT JSON_LENGTH('\$$array_var')`
    --let \$$stride_var = \$json_stride
    if (\$$stride_var == '') {
      --let \$$stride_var = 1
    }
    if (\$json_start == '') {
      if (\$$stride_var > 0) {
        --let \$$index_var = 0
      }
      if (\$$stride_var < 0) {
        --let \$$index_var = \$$length_var
        --dec \$$index_var
      }
    }
    if (\$json_start != '') {
      --let \$$index_var = \$json_start
    }
    if (\$rpl_debug) {
      --echo # $start_file_var: $array_var=<\$$array_var>
      --echo # $start_file_var: $length_var=<\$$length_var>
    }
    --source \$$next_file_var
  "##################################################
  ) or die "Error $? writing to $start_file: $!";
  close(START_FILE)
    or die "Error $? closing $start_file: $!";

  my $func = 'JSON_EXTRACT';
  my $func_end = '';
  if ($unquote =~ /^1?$/) {
    $func = "JSON_UNQUOTE($func";
    $end = ')'
  }
  open(NEXT_FILE, "> $next_file")
    or die "Error $? opening $next_file: $!";
  print NEXT_FILE (##############################################
    "
    if (\$rpl_debug) {
      --echo # $next_file_var: $index_var=<\$$index_var> $length_var=\$$length_var
    }
    --let \$$done_var = 0
    if (\$$index_var >= \$$length_var) {
      --let \$$done_var = 1
    }
    if (\$$index_var < 0) {
      --let \$$done_var = 1
    }
    if (!\$$done_var) {
      --let \$$value_var = `SELECT $func('\$$array_var', '\$[\$$index_var]'$end)`
      --expr \$$index_var = \$$index_var + \$$stride_var
      if (\$rpl_debug) {
        --echo # $next_file_var: $value_var=<\$$value_var>
      }
    }
    if (\$rpl_debug) {
      --echo # $next_file_var: $done_var=<\$$done_var>
    }
  "#############################################################
  ) or die "Error $? writing to $next_file: $!";
  close(NEXT_FILE)
    or die "Error $? closing $next_file: $!";
END_OF_PERL

--let $cji_init = _init_iterator.inc
--source $MYSQLTEST_VARDIR/tmp/json_$json_label$cji_init
