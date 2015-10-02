
#
# This include will remove a debug point from the current GLOBAL or SESSION
# debug variable without changing other debugging flags set.
#
# ==== Usage ====
#
# --let $debug_point= debug_point
# [--let $debug_point_silent= 1]
# [--let $debug_type= GLOBAL | SESSION]
# --source extra/rpl_tests/remove_debug_point.inc
#
# Parameters:
#   $debug_point
#     The debug point to be activated.
#
#   $debug_point_silent
#     By default, this script prints the name of the debug point. If
#     this parameter is set, it does not print anything.
#
#   $debug_type
#     If the debug variable to be changed is the GLOBAL or SESSION one.
#     The default (if not specified one) is GLOBAL.
#

--let $_previous_include_silent= $include_silent
--let $include_silent= 1
--let $include_filename= remove_debug_point.inc
--source include/begin_include_file.inc
--let $include_silent= $_previous_include_silent

if (!$debug_type)
{
  --let $_debug_type= GLOBAL
}
if ($debug_type)
{
  --let $_debug_type= $debug_type
}
if ( `SELECT UPPER('$_debug_type') <> 'GLOBAL' AND UPPER('$_debug_type') <> 'SESSION'` )
{
  --die ERROR IN TEST: invalid value for mysqltest variable 'debug_type': $debug_type
}
if (!$debug_point)
{
  --die ERROR IN TEST: the mysqltest variable 'debug_point' must be set
}

--disable_query_log

--eval SET @previous_debug=@@$_debug_type.debug
--eval SET @new_debug=@@$_debug_type.debug

if (`SELECT LOCATE('$debug_point', @previous_debug) > 0`)
{
  --eval SET @new_debug=REPLACE(@previous_debug, ',$debug_point', '')

  # If there is no other debug options, we will clean it up
  if (`SELECT @new_debug="d"`)
  {
    SET @new_debug="";
  }
}

if (`SELECT @new_debug = @previous_debug`)
{
  --die ERROR IN TEST: the debug point '$debug_point' was not found
}

if (!$debug_point_silent)
{
  --let $message_prefix=`SELECT REPEAT("#", $_include_file_depth)`
  --echo $message_prefix Removing debug point '$debug_point' from @@$_debug_type.debug
}

if ($rpl_debug)
{
  --let $_to_display=`SELECT @@$_debug_type.debug`
  --echo @@$_debug_type.debug was '$_to_display'
}

--eval SET @@$_debug_type.debug=@new_debug

if ($rpl_debug)
{
  --let $_to_display=`SELECT @@$_debug_type.debug`
  --echo @@$_debug_type.debug set to '$_to_display'
}

--let $include_filename= remove_debug_point.inc
--source include/end_include_file.inc
