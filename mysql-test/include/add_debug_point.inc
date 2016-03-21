# ==== Purpose ====
#
# This include will add a debug point to the current GLOBAL or SESSION
# debug variable without changing other debugging flags set.
#
# This is useful when generating debug trace for test cases that uses
# debug point features. By using this include the trace generation will
# not be stopped.
#
# ==== Usage ====
#
# --let $debug_point= debug_point
# [--let $debug_point_silent= 1]
# [--let $debug_type= GLOBAL | SESSION]
# --source include/add_debug_point.inc
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
--let $include_filename= add_debug_point.inc
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

--let $_debug_point= $debug_point
if (!$rpl_debug)
{
  --disable_query_log
}

--eval SET @previous_debug = @@$_debug_type.debug;
SET @before_debug_point='';
SET @after_debug_point='';

# check if debug is empty
if ( `SELECT @previous_debug = ''` )
{
  SET @before_debug_point='d,';
}

# check if debug is 'd'
if ( `SELECT @previous_debug = 'd'` )
{
  SET @before_debug_point='d,';
}

# check if debug starts with 'd:'
if ( `SELECT INSTR(@previous_debug,'d:') = 1` )
{
  SET @before_debug_point='d,';
  SET @after_debug_point=RIGHT(@previous_debug, LENGTH(@previous_debug) - 1);
}

# check if debug starts with 'd,' and has no ':'
if ( `SELECT INSTR(@previous_debug,'d,') = 1 AND INSTR(@previous_debug,':') = 0` )
{
  SET @before_debug_point=CONCAT(@previous_debug, ',');
}

# check if debug starts with 'd,' and has ':'
if ( `SELECT INSTR(@previous_debug,'d,') = 1 AND INSTR(@previous_debug,':') > 0` )
{
  SET @before_debug_point=LEFT(@previous_debug, INSTR(@previous_debug,':') - 1);
  SET @after_debug_point=RIGHT(@previous_debug, LENGTH(@previous_debug) - INSTR(@previous_debug,':') + 1);
  --let $_debug_point= ,$_debug_point
}

if (!$debug_point_silent)
{
  --let $message_prefix=`SELECT REPEAT("#", $_include_file_depth)`
  --echo $message_prefix Adding debug point '$debug_point' to @@$_debug_type.debug
}

if ($rpl_debug)
{
  --let $_to_display=`SELECT @@$_debug_type.debug`
  --echo @@$_debug_type.debug was '$_to_display'
}

--eval SET @@$_debug_type.debug=CONCAT(@before_debug_point, '$_debug_point', @after_debug_point)

if ($rpl_debug)
{
  --let $_to_display=`SELECT @@$_debug_type.debug`
  --echo @@$_debug_type.debug set to '$_to_display'
}

--let $include_filename= add_debug_point.inc
--source include/end_include_file.inc
