# ==== Purpose ====
#
# See include/begin_include_file.inc
#
#
# ==== Usage ====
#
# # At the end of include/my_file.inc:
# --let $include_filename= my_file.inc
# [--let $skip_restore_connection= 1]
# [--let $rpl_debug= 1]
# --source include/begin_include_file.inc
#
# Parameters:
#   $include_filename
#     Name of file that sources this file.
#
#   $skip_restore_connection
#     By default, this script restores the connection that was active
#     when begin_include_file.inc was sourced.  If
#     $skip_restore_connection is set, then this step is skipped and
#     end_include_file.inc leaves the connection as it was before
#     end_include_file.inc was sourced.

--let $_tmp= `SELECT SUBSTRING('$_include_file_enabled_warnings', 1, 1)`
--let $_include_file_enabled_warnings= `SELECT SUBSTRING('$_include_file_enabled_warnings', 2)`
if ($_tmp) {
  if ($_tmp != $ENABLE_WARNINGS) {
    # Enable all the disabled warnings
    --enable_warnings
  }
}

if (!$_tmp) {
  if ($_include_file_disable_warnings_list) {
    --let $_include_file_disable_warnings = `SELECT SUBSTRING('$_include_file_disable_warnings_list', 1, LOCATE('|', '$_include_file_disable_warnings_list') - 1)`
    --let $_include_file_disable_warnings_list = `SELECT SUBSTRING('$_include_file_disable_warnings_list', LOCATE('|', '$_include_file_disable_warnings_list') + 1)`

    # Check if there exist any disabled warnings
    if ($_include_file_disable_warnings) {
      # Disabled warnings are present, enable all the warings if $ENABLE_WARNINGS
      # is not set and then disable only the warnings present in disabld warning list.
      if (!$ENABLE_WARNINGS) {
        --enable_warnings
      }
      --disable_warnings $_include_file_disable_warnings
    }

    if (!$_include_file_disable_warnings) {
      # No disabled warnings exist, check if any enabled warnings present
      --let $_include_file_enable_warnings = `SELECT SUBSTRING('$_include_file_enable_warnings_list', 1, LOCATE('|', '$_include_file_enable_warnings_list') - 1)`
      --let $_include_file_enable_warnings_list = `SELECT SUBSTRING('$_include_file_enable_warnings_list', LOCATE('|', '$_include_file_enable_warnings_list') + 1)`

      if ($_include_file_enable_warnings) {
        # Enabled warnings exist, disable all the warnings first if $ENABLE_WARNINGS
        # is set and then enable only the warnings present in enabled warnings list.
        if ($ENABLE_WARNINGS) {
          --disable_warnings
        }
        --enable_warnings $_include_file_enable_warnings
      }

      if (!$_include_file_enable_warnings) {
        # There exist no disabled or enabled warnings, disable all
        # the warnings.
        --disable_warnings
      }
    }
  }
}

--let $_tmp= `SELECT SUBSTRING('$_include_file_enabled_query_log', 1, 1)`
--let $_include_file_enabled_query_log= `SELECT SUBSTRING('$_include_file_enabled_query_log', 2)`
if ($_tmp) {
  --enable_query_log
}
if (!$_tmp) {
  --disable_query_log
}

--let $_tmp= `SELECT SUBSTRING('$_include_file_enabled_result_log', 1, 1)`
--let $_include_file_enabled_result_log= `SELECT SUBSTRING('$_include_file_enabled_result_log', 2)`
if ($_tmp) {
  --enable_result_log
}
if (!$_tmp) {
  --disable_result_log
}

--let $_tmp= `SELECT SUBSTRING('$_include_file_enabled_abort_on_error', 1, 1)`
--let $_include_file_enabled_abort_on_error= `SELECT SUBSTRING('$_include_file_enabled_abort_on_error', 2)`
if ($_tmp) {
  --enable_abort_on_error
}
if (!$_tmp) {
  --disable_abort_on_error
}

--let $_include_file_indent= `SELECT REPEAT('.', $_include_file_depth - 1)`

--let $_include_file_rpl_connection_name= `SELECT SUBSTRING_INDEX('$_include_file_connection', ',', 1)`
--let $_include_file_connection= `SELECT SUBSTRING('$_include_file_connection', LENGTH('$_include_file_rpl_connection_name') + 2)`
if (!$skip_restore_connection)
{
  --let $rpl_connection_name= $_include_file_rpl_connection_name
  --source include/rpl_connection.inc
}
--let $skip_restore_connection= 0

--dec $_include_file_depth

if ($rpl_debug)
{
  --echo $_include_file_indent==== END include/$include_filename ====
  --echo $_include_file_indent con='$CURRENT_CONNECTION' warn='$ENABLE_WARNINGS' qlog='$ENABLE_QUERY_LOG' rlog='$ENABLE_RESULT_LOG' aborterr='$ENABLE_ABORT_ON_ERROR'
}
--let $include_filename=
