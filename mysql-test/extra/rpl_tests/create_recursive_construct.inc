# ==== Purpose ====
#
# Creates a stored routine, stored function, trigger, view, or
# prepared statement (commonly referred to as "recursive construct")
# that invokes a given unsafe statement.
#
# Then, it invokes the created recursive construct several times:
#
#  - With SQL_LOG_BIN = 1 and binlog_format = STATEMENT, to verify
#    that it gives a warning.
#
#  - With SQL_LOG_BIN = 0 and binlog_format = STATEMENT, to verify that
#    there is no warning and nothing is logged.
#
#  - With SQL_LOG_BIN = 1 and binlog_format = MIXED, to verify that it
#    writes row events to the binlog.
#
#  - In some cases, the recursive construct can be invoked so that it
#    has no side-effects but returns a value that may be
#    nondeterministic.  An example is a function that returns UUID().
#    The function does not have side effects but its a return value
#    that may differ on slave.  Such statements are invoked so that
#    the return value is discarded (e.g., SELECT func()), with
#    SQL_LOG_BIN = 1 and binlog_format = STATEMENT.  In this case, no
#    warning should be given and nothing should be written to the
#    binlog.
#
# This is an auxiliary file particularly targeted to being used by the
# test binlog_unsafe.  In this context, the purpose is to check how
# warnings for unsafe statements are propagated in recursive
# constructs.
#
# The statement to invoke ("input") is described using mtr variables,
# and the resulting recursive construct ("output") is stored in mtr
# variables in a similar fashion.  To create several levels of nested
# recursive constructs, source this file once, then copy the values of
# appropriate output variables to the input variables, and then source
# this file again.
#
#
# ==== Usage ====
#
# See binlog_unsafe for an example of how to use this file.
#
# let $CRC_ARG_level= <level>;
# let $CRC_ARG_type= <type>;
# let $CRC_ARG_stmt_sidef= <stmt>;
# let $CRC_ARG_value= <stmt>;
# let $CRC_ARG_sel_retval= <stmt>;
# let $CRC_ARG_sel_sidef= <stmt>;
# let $CRC_ARG_desc= <desc>;
# source extra/rpl_tests/create_recursive_construct.inc;
# let $my_stmt_sidef= $CRC_RET_stmt_sidef;
# let $my_value= $CRC_RET_value;
# let $my_sel_sidef= $CRC_RET_sel_sidef;
# let $my_sel_retval= $CRC_RET_sel_retval;
# let $my_drop= $CRC_RET_drop;
# let $my_is_toplevel= $CRC_RET_top_is_toplevel;
# let $my_desc= $CRC_RET_desc;
#
# $CRC_ARG_* are used as input parameters (arguments) to this file:
#
#   $CRC_ARG_level is the recursion depth: 1 for the innermost
#   statement created, 2 for a statement that invokes a statement on
#   level 1, etc.
#
#   $CRC_ARG_type is an integer from 0 to 6, indicating what type of
#   statement shall be created:
#    0 - Create a stored function where the return value depends on
#        the value of the given statement.
#    1 - Create a stored function that invokes the given statement as
#        a side-effect but may not return a value that depends on it.
#    2 - Create a stored routine that invokes the given statement.
#    3 - Create a trigger (on table trigger_table_$CRC_ARG_level) that
#        invokes the given statement.
#    4 - Create a view that returns a value that depends on the value
#        of the given statement.
#    5 - Create a view that invokes the given statement but may return
#        a value that does not depend on it.
#    6 - Create a prepared statement that invokes the given statement.
#
#   $CRC_ARG_stmt_sidef is the statement to invoke.  It should be a
#   statement that can be invoked on its own (not sub-statement),
#   which causes something unsafe to be written to the binlog.
#
#   $CRC_ARG_value is a sub-statement holding the value of the given
#   statement.  Can be empty if the given statement does not have a
#   value.  Typically, this is non-empty if the given statement is a
#   function call or user variable, but not if it is a stored routine
#   call, INSERT, SELECT, etc (because none of them has a value).
#   $CRC_ARG_value is used only when $CRC_ARG_type=6.
#
#   $CRC_ARG_sel_sidef is a SELECT sub-statement that invokes the
#   statement as a side-effect, but returns a result set that may not
#   depend on the statement.  Can be empty if the statement cannot
#   produce a result set from a SELECT.  $CRC_ARG_sel_sidef is used
#   only if $CRC_ARG_type=2
#
#   $CRC_ARG_sel_retval is a SELECT sub-statement that does not have
#   side-effects, but returns a result set that depends on the unsafe
#   statement.  Can be empty if the statement cannot be invoked from a
#   SELECT.  $CRC_ARG_sel_retval is used only if $CRC_ARG_type=3.
#
#   $CRC_ARG_desc is a human-readable description of the statement to
#   invoke.
#
# $CRC_RET_* are used as output parameters (return values) of this
# file:
#
#   $CRC_RET_stmt_sidef is a statement invoking the resulting recursive
#   construct.
#
#   $CRC_RET_value is a sub-statement invoking the resulting recursive
#   construct and returning the value of the recursive construct.
#   This is the empty string if the resulting recursive construct does
#   not have a value.  In particular, this is non-empty only if
#   $CRC_ARG_value=7.
#
#   $CRC_RET_sel_sidef is a SELECT sub-statement that invokes the
#   resulting recursive construct as a side-effect but where the
#   result set may not depend on the recursive construct.  This is the
#   empty string if the recursive construct cannot be invoked from a
#   SELECT.  In particular, this is non-empty only if $CRC_ARG_value=6
#   or $CRC_ARG_value=2.
#
#   $CRC_RET_sel_retval is a SELECT sub-statement that does not have
#   side-effects, but returns a result set depending on the unsafe
#   statement.  This is the empty string if the recursive construct
#   cannot produce a result set from a SELECT.  In particular, this is
#   non-empty only if $CRC_ARG_value=7 or $CRC_ARG_value=3.
#
#   $CRC_RET_drop is a statement that drops the created object.  I.e.,
#   it is one of 'DROP FUNCTION <func>', 'DROP PROCEDURE <proc>', etc.
#
#   $CRC_RET_top_is_toplevel is 0 normally, or 1 if the resulting
#   recursive construct can only be called from a top-level statement.
#   In particular, this is 1 only when $CRC_ARG_value=1, because
#   prepared statements cannot be invoked from other recursive
#   constructs.
#
#   $CRC_RET_desc is a text string that describes the invokation of
#   the recursive construct in a human-readable fashion.
#
# Assumptions
#
#   Before sourcing this file with $CRC_ARG_level=X, you need to
#   create three tables: tX, taX and trigger_table_X.  These are used
#   as auxiliary tables.


#--echo debug: >>>>ENTER create_recursive_construct
#--echo debug: level=$CRC_ARG_level
#--echo debug: type=$CRC_ARG_type
#--echo debug: stmt_sidef=$CRC_ARG_stmt_sidef
#--echo debug: value=$CRC_ARG_value
#--echo debug: sel_retval=$CRC_ARG_sel_retval
#--echo debug: sel_sidef=$CRC_ARG_sel_sidef

--let $CRC_RET_stmt_sidef=
--let $CRC_RET_value=
--let $CRC_RET_sel_retval=
--let $CRC_RET_sel_sidef=
--let $CRC_RET_drop=
--let $CRC_RET_is_toplevel= 1
--let $CRC_RET_desc=
--let $CRC_name=
--let $CRC_create=

######## func_retval ########
# if inside if in lieu of AND operand
if ($CRC_ARG_type == 0) {
 if ($CRC_ARG_value) {
  # It will be safe to call this function and discard the return
  # value, but it will be unsafe to use return value (e.g., in
  # INSERT...SELECT).
  --let $CRC_name= func_retval_$CRC_ARG_level
  --let $CRC_create= CREATE FUNCTION $CRC_name() RETURNS VARCHAR(100) BEGIN INSERT INTO ta$CRC_ARG_level VALUES (47); RETURN $CRC_ARG_value; END
  --let $CRC_RET_stmt_sidef= INSERT INTO t$CRC_ARG_level VALUES ($CRC_name())
  --let $CRC_RET_value= $CRC_name()
  --let $CRC_RET_sel_sidef=
  --let $CRC_RET_sel_retval= SELECT $CRC_name()
  --let $CRC_RET_drop= DROP FUNCTION $CRC_name
  --let $CRC_RET_is_toplevel= 0
  --let $CRC_RET_desc= function $CRC_name returning value from $CRC_ARG_desc
 }
}

######## func_sidef ########
if ($CRC_ARG_type == 1) {
  # It will be unsafe to call func even if you discard return value.
  --let $CRC_name= func_sidef_$CRC_ARG_level
  --let $CRC_create= CREATE FUNCTION $CRC_name() RETURNS VARCHAR(100) BEGIN INSERT INTO ta$CRC_ARG_level VALUES (47); $CRC_ARG_stmt_sidef; RETURN 0; END
  --let $CRC_RET_stmt_sidef= INSERT INTO t$CRC_ARG_level SELECT $CRC_name()
  --let $CRC_RET_value=
  --let $CRC_RET_sel_retval=
  --let $CRC_RET_sel_sidef= SELECT $CRC_name()
  --let $CRC_RET_drop= DROP FUNCTION $CRC_name
  --let $CRC_RET_is_toplevel= 0
  --let $CRC_RET_desc= function $CRC_name invoking $CRC_ARG_desc
}

######## proc ########
if ($CRC_ARG_type == 2) {
  # It will be unsafe to call this procedure.
  --let $CRC_name= proc_$CRC_ARG_level
  --let $CRC_create= CREATE PROCEDURE $CRC_name() BEGIN $CRC_ARG_stmt_sidef; INSERT INTO ta$CRC_ARG_level VALUES (47); END
  --let $CRC_RET_stmt_sidef= CALL $CRC_name()
  --let $CRC_RET_value=
  --let $CRC_RET_sel_retval=
  --let $CRC_RET_sel_sidef=
  --let $CRC_RET_drop= DROP PROCEDURE $CRC_name
  --let $CRC_RET_is_toplevel= 0
  --let $CRC_RET_desc= procedure $CRC_name invoking $CRC_ARG_desc
}

######## trig ########
if ($CRC_ARG_type == 3) {
  # It will be unsafe to invoke this trigger.
  --let $CRC_name= trig_$CRC_ARG_level
  --let $CRC_create= CREATE TRIGGER $CRC_name BEFORE INSERT ON trigger_table_$CRC_ARG_level FOR EACH ROW BEGIN INSERT INTO ta$CRC_ARG_level VALUES (47); $CRC_ARG_stmt_sidef; END
  --let $CRC_RET_stmt_sidef= INSERT INTO trigger_table_$CRC_ARG_level VALUES (1)
  --let $CRC_RET_value=
  --let $CRC_RET_sel_retval=
  --let $CRC_RET_sel_sidef=
  --let $CRC_RET_drop= DROP TRIGGER $CRC_name
  --let $CRC_RET_is_toplevel= 0
  --let $CRC_RET_desc= trigger $CRC_name invoking $CRC_ARG_desc
}

######## view_retval ########
if ($CRC_ARG_type == 4) {
 if ($CRC_ARG_sel_retval) {
  # It will be safe to select from this view if you discard the result
  # set, but unsafe to use result set (e.g., in INSERT..SELECT).
  --let $CRC_name= view_retval_$CRC_ARG_level
  --let $CRC_create= CREATE VIEW $CRC_name AS $CRC_ARG_sel_retval
  --let $CRC_RET_stmt_sidef= INSERT INTO t$CRC_ARG_LEVEL SELECT * FROM $CRC_name
  --let $CRC_RET_value=
  --let $CRC_RET_sel_retval= SELECT * FROM $CRC_name
  --let $CRC_RET_sel_sidef=
  --let $CRC_RET_drop= DROP VIEW $CRC_name
  --let $CRC_RET_is_toplevel= 0
  --let $CRC_RET_desc= view $CRC_name returning value from $CRC_ARG_desc
 }
}

######## view_sidef ########
if ($CRC_ARG_type == 5) {
 if  ($CRC_ARG_sel_sidef) {
  # It will be unsafe to select from this view, even if you discard
  # the return value.
  --let $CRC_name= view_sidef_$CRC_ARG_level
  --let $CRC_create= CREATE VIEW $CRC_name AS $CRC_ARG_sel_sidef
  --let $CRC_RET_stmt_sidef= INSERT INTO t$CRC_ARG_level SELECT * FROM $CRC_name
  --let $CRC_RET_value=
  --let $CRC_RET_sel_retval=
  --let $CRC_RET_sel_sidef= SELECT * FROM $CRC_name
  --let $CRC_RET_drop= DROP VIEW $CRC_name
  --let $CRC_RET_is_toplevel= 0
  --let $CRC_RET_desc= view $CRC_name invoking $CRC_ARG_desc
 }
}

######## prep ########
if ($CRC_ARG_type == 6) {
  # It will be unsafe to execute this prepared statement
  --let $CRC_name= prep_$CRC_ARG_level
  --let $CRC_create= PREPARE $CRC_name FROM "$CRC_ARG_stmt_sidef"
  --let $CRC_RET_stmt_sidef= EXECUTE $CRC_name
  --let $CRC_RET_value=
  --let $CRC_RET_sel_retval=
  --let $CRC_RET_sel_sidef=
  --let $CRC_RET_drop= DROP PREPARE $CRC_name
  --let $CRC_RET_is_toplevel= 1
  --let $CRC_RET_desc= prepared statement $CRC_name invoking $CRC_ARG_desc
}

######## no recursive construct: just return the given statement ########
if ($CRC_ARG_type == 7) {
  # CRC_ARG_type=7 is a special case. We just set $CRC_RET_x =
  # $CRC_ARG_x.  This way, the $CRC_ARG_stmt gets executed directly
  # (below).  In binlog_unsafe.test, it is used to invoke the unsafe
  # statement created in the outermost loop directly, without
  # enclosing it in a recursive construct.
  --let $CRC_RET_stmt_sidef= $CRC_ARG_stmt_sidef
  --let $CRC_RET_value= $CRC_ARG_value
  --let $CRC_RET_sel_retval= $CRC_ARG_sel_retval
  --let $CRC_RET_sel_sidef= $CRC_ARG_sel_sidef
  --let $CRC_RET_drop=
  --let $CRC_RET_is_toplevel= 1
  --let $CRC_RET_desc= $CRC_ARG_desc
}

######## execute! ########
if ($CRC_RET_stmt_sidef) {
  --echo
  --echo Invoking $CRC_RET_desc.
  if ($CRC_create) {
    --eval $CRC_create
  }

  --echo * binlog_format = STATEMENT: expect $CRC_ARG_expected_number_of_warnings warnings.
  --eval $CRC_RET_stmt_sidef
  --let $n_warnings= `SHOW COUNT(*) WARNINGS`
  if ($n_warnings != $CRC_ARG_expected_number_of_warnings) {
    --echo ******** Failure! Expected $CRC_ARG_expected_number_of_warnings warnings, got $n_warnings warnings. ********
    SHOW WARNINGS;
    SHOW BINLOG EVENTS;
    --die Wrong number of warnings.
  }

  # These queries are run without query log, to make result file more
  # readable.  Debug info is only printed if something abnormal
  # happens.
  --disable_query_log

  --echo * SQL_LOG_BIN = 0: expect nothing logged and no warning.
  SET SQL_LOG_BIN = 0;
  RESET MASTER;
  --eval $CRC_RET_stmt_sidef
  --let $n_warnings= `SHOW COUNT(*) WARNINGS`
  if ($n_warnings) {
    --echo ******** Failure! Expected 0 warnings, got $n_warnings warnings. ********
    SHOW WARNINGS;
    SHOW BINLOG EVENTS;
    --die Wrong number of warnings.
  }
  --let $binlog_event= query_get_value(SHOW BINLOG EVENTS, Event_type, 2)
  if ($binlog_event != No such row) {
    --enable_query_log
    --echo ******** Failure! Something was written to the binlog despite SQL_LOG_BIN=0 ********
    SHOW BINLOG EVENTS;
    --die Binlog not empty
  }
  SET SQL_LOG_BIN = 1;

  --echo * binlog_format = MIXED: expect row events in binlog and no warning.
  SET binlog_format = MIXED;
  RESET MASTER;
  --eval $CRC_RET_stmt_sidef
  --let $n_warnings= `SHOW COUNT(*) WARNINGS`
  if ($n_warnings) {
    --echo ******** Failure! Expected 0 warnings, got $n_warnings warnings. ********
    SHOW WARNINGS;
    SHOW BINLOG EVENTS;
    --die Warnings printed
  }
  --let $event_type= query_get_value(SHOW BINLOG EVENTS, Event_type, 3)
  # The first event is format_description, the second is
  # Query_event('BEGIN'), and the third should be our Query
  # for 'INSERT DELAYED' unsafe_type 3, which is safe after
  # the fix of bug#54579.
  if (`SELECT $unsafe_type = 3 AND '$event_type' != 'Query'`) {
    --enable_query_log
    --echo ******** Failure! Event number 3 was a '$event_type', not a 'Query'. ********
    SHOW BINLOG EVENTS;
    --die Wrong events in binlog.
  }
  # The first event is format_description, the second is
  # Query_event('BEGIN'), and the third should be our Table_map
  # for unsafe statement.
  if (`SELECT $unsafe_type != 3 AND '$event_type' != 'Table_map'`) {
    --enable_query_log
    --echo ******** Failure! Event number 3 was a '$event_type', not a 'Table_map'. ********
    SHOW BINLOG EVENTS;
    --die Wrong events in binlog.
  }
  SET binlog_format = STATEMENT;

  --enable_query_log
}

# Invoke created object, discarding the return value. This should not
# give any warning.
if ($CRC_RET_sel_retval) {
  --echo * Invoke statement so that return value is dicarded: expect no warning.
  --disable_result_log
  --eval $CRC_RET_sel_retval
  --enable_result_log

  # Currently, due to a bug, we do get warnings here, so we don't
  # fail.  When the bug is fixed, we should execute the following.

  #--let $n_warnings= `SHOW COUNT(*) WARNINGS`
  #if ($n_warnings) {
  #  --enable_query_log
  #  --echo Failure! Expected 0 warnings, got $n_warnings warnings.
  #  SHOW WARNINGS;
  #  SHOW BINLOG EVENTS;
  #  --die Wrong number of warnings.
  #}
}

#--echo debug: <<<<EXIT create_recursive_construct
#--echo debug: stmt_sidef=$CRC_RET_stmt_sidef
#--echo debug: value=$CRC_RET_value
#--echo debug: sel_retval=$CRC_RET_sel_retval
#--echo debug: sel_sidef=$CRC_RET_sel_sidef
#--echo debug: drop=$CRC_RET_drop
#--echo debug: is_toplevel=$CRC_RET_is_toplevel
#--echo debug: desc=$CRC_RET_desc
