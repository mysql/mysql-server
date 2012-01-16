# ==== Purpose ====
#
# This is an auxiliary file that facilitates writing include/*.inc
# files. It has three purposes:
#
#  1. Store mtr's state at the beginning of the .inc file and restore
#     the state at the end. The following status is restored:
#
#     disable_warnings
#     disable_query_log
#     disable_result_log
#     disable_abort_on_errors
#     Current connection
#
#  2. This file also prints the name of the .inc file that sources
#     it. Only the name of the top-level .inc file is printed: if
#     file_1.inc sources file_2.inc, then this file only prints
#     file_1.inc.
#
#  3.  If the mysqltest variable $rpl_debug is set, then
#      this file will print:
#
#        ==== BEGIN include/<filename> ====
#
#      and end_include_file.inc will print
#
#        ==== END include/<filename> ====
#
#      These printouts are indented to make it easier to read the
#      result log.
#
#
# ==== Usage ====
#
# # At the beginning of include/my_file.inc:
# --let $include_filename= my_file.inc
# [--let $rpl_debug= 1]
# --source include/begin_include_file.inc
#
# # At the end of include/my_file.inc:
# --let $include_filename= my_file.inc
# --source include/end_include_file.inc
#
# Parameters:
#   $include_filename
#     The basename of the file: a file named /path/to/my_file.inc
#     should set $include_filename=my_file.inc.  This parameter
#     must be provided both for begin_include_file.inc and
#     end_include_file.inc.
#
#   $keep_include_silent
#     This specifies whether it should be echoed to the result file
#     the following string: include/$include_filename
#     If not set, the string will be echoed.
#
#   $rpl_debug
#     If set, this script will print the following text:
#      ==== BEGIN include/$include_filename.inc ====


# Print 'include/$include_filename', but only when invoked from
# the top-level. We don't want to print
# 'include/$include_filename' from all files included
# recursively.
if (!$_include_file_depth)
{
  if (!$keep_include_silent)
  {
    --echo include/$include_filename
  }
  --let $_include_file_depth= 0
}
--inc $_include_file_depth
if ($rpl_debug)
{
  --echo $_include_file_indent==== BEGIN include/$include_filename ====
}

--let $_include_file_enabled_warnings= $ENABLED_WARNINGS$_include_file_enabled_warnings
--let $_include_file_enabled_query_log= $ENABLED_QUERY_LOG$_include_file_enabled_query_log
--let $_include_file_enabled_result_log= $ENABLED_RESULT_LOG$_include_file_enabled_result_log
--let $_include_file_enabled_abort_on_error= $ENABLED_ABORT_ON_ERROR$_include_file_enabled_abort_on_error
--let $_include_file_connection= $CURRENT_CONNECTION,$_include_file_connection

if ($rpl_debug)
{
  --echo $_include_file_indent con='$CURRENT_CONNECTION' warn='$ENABLED_WARNINGS' qlog='$ENABLED_QUERY_LOG' rlog='$ENABLED_RESULT_LOG' aborterr='$ENABLED_ABORT_ON_ERROR'
}

--let $include_filename=
--let $_include_file_indent= .$_include_file_indent
