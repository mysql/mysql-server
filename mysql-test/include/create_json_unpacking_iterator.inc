# ==== Purpose ====
#
# Create an 'unpacking iterator', which is capable of iterating over a
# JSON array where the elements are JSON objects, and in each
# iteration unpack the object into a set of mysqltest variables.
#
# The unpacking iterator consists of two files: one to start
# iteration, and one to step forward. The set of keys to unpack is
# defined when creating the unpacking iterator, and the iterator is
# bound to a JSON array each time the start file is sourced.  This
# script exposes the two file names to the caller through two mysqltest
# variables.
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
# --source include/create_json_unpacking_iterator.inc
#
# # Now:
# # - To start iteration, set $json_array to the JSON array which you need
# #   to iterate over, and then source the file $json_NAME_start.
# # - To check when iteration is complete, check $json_NAME_done.
# # - To inspect the current values, inspect $json_KEY1, $json_KEY2, etc.
# # - To jump to next set of values, source $json_NAME_next.
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
#   For all other parameters: see create_json_unpacker.inc
#
# Output:
#
#   This script generates two .inc files in $MYSQLTEST_VARDIR/tmp,
#   which can be used to iterate over a JSON array in which each
#   element is JSON object, and in each iteration unpack the object
#   into a set of mysqltest variables.  If $json_label is NAME, the
#   script names are stored in the mysqltest variables
#   $json_NAME_start and $json_NAME_next.  These scripts should be
#   used as follows:
#
#   --let $json_array = JSON_ARRAY
#   [--let $json_start = NUMBER]
#   [--let $json_stride = NUMBER]
#   --source $json_NAME_start
#
#     Start the iteration.  See create_json_iterator.inc for details
#     about the optional parameters $json_start and $json_stride.  It
#     will set $json_NAME_value to the first element of the array, and
#     it will assume that the first element is a JSON object and
#     unpack it into mtr variables.  The unpacking is done just like
#     for create_json_unpacker.inc, using the parameters specified for
#     create_json_unpacking_iterator.inc; see
#     create_json_unpacker.inc.
#
#   [--let $json_NAME_index = NUMBER]
#   [--let $json_NAME_stride = NUMBER]
#   --source $json_NAME_next
#
#     Move to the next iteration. If it reached the end of the array,
#     it sets $json_NAME_done; otherwise it unpacks the value just
#     like $json_NAME_start did.
#
# ==== Examples ====
#
# See mysql-test/t/mysqltest_json.test.

--source include/create_json_iterator.inc
--source include/create_json_unpacker.inc

--let CJUI_LABEL = $json_label
perl END_OF_PERL;
  my $label = $ENV{'CJUI_LABEL'};
  my $prefix = $ENV{'MYSQLTEST_VARDIR'} . "/tmp/json_$label";
  my $next = $prefix . '_next.inc';
  my $unpack = $prefix . '_unpack.inc';
  my $done_var = "json_${label}_done";
  my $value_var = "json_${label}_value";
  open(NEXT, ">> $next")
    or die "Error $? opening $next: $!";
  print NEXT (
    "if (!\$$done_var) {\n" .
    "  --let \$json_object = \$$value_var\n" .
    "  --source $unpack\n" .
    "}\n"
  ) or die "Error $? writing to $next: $!";
  close(NEXT)
    or die "Error $? closing $next: $!";
END_OF_PERL
