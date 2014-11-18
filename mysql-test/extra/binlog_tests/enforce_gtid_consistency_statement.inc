# ==== Purpose ====
#
# This is an auxiliary test script used by
# extra/binlog_tests/enforce_gtid_consistency.test. It tests that
# GTID-consistency violation error/warning is generated correctly for
# a statement.
#
# ==== Usage ====
#
# [--let $pre_statement= STATEMENT]
# --let $gtid_next= VALUE
# --let $statement= STATEMENT
# --let $gtid_violation= [0|1]
# --let $violation_result= [0|1|2]
# --let $error= ER_SOMETHING
# --let $error= some text
#
# Parameters:
#
#   $pre_statement, $gtid_next, $statement
#     This script will do the following:
#      1. Set GTID_NEXT='$gtid_next'. If $gtid_next is equal to GTID
#         (the four letters G, T, I, and D), then the script will
#         generate a GTID to use.
#      2. Execute $pre_statement, if it is not empty.
#      3. Execute $statement.
#
#   $gtid_violation
#     Set this to 1 if it is expected that $statement generates a
#     GTID violation, 0 if not.
#
#   $violation_result
#     Just because there is a GTID violation, it does not have to
#     result in an error.  It could go through without problems
#     (e.g. if gtid_mode=off and enforce_gtid_consistency=off) or it
#     could generate a warning (e.g. if gtid_next!=UUID:NUMBER and
#     enforce_gtid_consistency=WARN) or it could generate an error
#     (e.g. if enforce_gtid_consistency=ON or gtid_next=UUID:NUMBER).
#     $violation_result specifies what is expected if this is a GTID
#     violation: 0 means no problem, 1 means error, and 2 means
#     warning.
#
#   $expect_counter_value
#     If there is a violation but it does not result in an error
#     (i.e., $gtid_violation==1 and $violation_result!=1), then the
#     global counter ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT or
#     AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should normally
#     increase by 1 for the duration of the transaction. Which of the
#     counters increases, depends on GTID_NEXT, and this script
#     automatically determines which variable it is using $gtid_next.
#     $expect_counter_value specifies the expected value of the
#     counter after $statement. Normally, if $statement ends the
#     transaction this value should be 0 since the commit decreases
#     the counter, whereas if $statement leaves the transaction open
#     this value should be
#
#   $expect_counter_value_row
#     Normally, $expect_counter_value is used regardless of
#     BINLOG_FORMAT.  But in some corner cases, the value of
#     $expect_counter_value depends on BINLOG_FORMAT.  For those
#     cases, the caller should set $expect_counter_value to the
#     expected value when BINLOG_FORMAT==STATEMENT or MIXED, and
#     $expect_counter_value_row to the expected value when
#     BINLOG_FORMAT=ROW.  The output to the result file will be
#     the same regardless of BINLOG_FORMAT.
#
#   $error_code
#     The expected error code (ER_*), in case $statement generates an
#     error.
#
#   $error_message
#     A substring of the error message text, in case $statement
#     generates an error.

--echo # enforce_gtid_consistency=$enforce_gtid_consistency gtid_mode=$gtid_mode gtid_next=$gtid_next

# $expectation is 0 if the statement is ok, 1 if an error is expected,
# 2 if a warning is expected.
--let $expectation= 0
if ($gtid_violation)
{
  --let $expectation= $violation_result
}

if ($rpl_debug)
{
  --echo gtid_violation='$gtid_violation' violation_result='$violation_result' error_code='$error_code' expectation='$expectation' expect_counter_value='$expect_counter_value' expect_counter_value_row='$expect_counter_value_row'
  --echo pre_statement='$pre_statement'
  --echo gtid_next='$gtid_next'
  --echo statement='$statement'
}

--let $binlog_format= `SELECT @@GLOBAL.BINLOG_FORMAT`
--source include/set_gtid_next_gtid_mode_agnostic.inc

if ($pre_statement != '')
{
  eval $pre_statement;
}

if ($expectation == 1)
{
  --echo error $error_code
  --replace_regex /[a-f0-9]{8}-[a-f0-9]{4}-[a-f0-9]{4}-[a-f0-9]{4}-[a-f0-9]{12}:[0-9]*/#/
  --error $error_code
  eval $statement;
}
if ($expectation != 1)
{
  eval $statement;
}

# Check warnings / errors.

--let $warning_count= `SHOW COUNT(*) WARNINGS`
--let $warning_message= query_get_value(SHOW WARNINGS, Message, 1)
--let $warning_level= query_get_value(SHOW WARNINGS, Level, 1)

--connection default

# The message may contain a single quote ('), which confuses
# include/assert.inc So we remove any single quotes.
--let $warning_message= `SELECT REPLACE("$warning_message", "'", "")`

if ($expectation == 0)
{
  --let $assert_text= No warning or error should be generated.
  --let $assert_cond= $warning_count = 0
  --let $extra_debug_info= $warning_message
  --source include/assert.inc
  --let $extra_debug_info=
}
if ($expectation == 1)
{
  --let $assert_text= One warning/error should be generated.
  --let $assert_cond= $warning_count = 1
  --source include/assert.inc

  --let $assert_text= It should be an Error, not a Warning.
  --let $assert_cond= "$warning_level" = "Error"
  --source include/assert.inc

  --let $assert_text= Text should be "violates GTID consistency"
  --let $assert_cond= "$warning_message" LIKE "$error_message%"
  --let $extra_debug_info= $warning_message
  --source include/assert.inc
  --let $extra_debug_info=
}
if ($expectation == 2)
{
  --let $assert_text= One warning/error should be generated.
  --let $assert_cond= $warning_count = 1
  --source include/assert.inc

  --let $assert_text= It should be a Warning, not an Error.
  --let $assert_cond= "$warning_level" = "Warning"
  --source include/assert.inc

  --let $assert_text= Text should be "violates GTID consistency"
  --let $assert_cond= "$warning_message" LIKE "Statement violates GTID consistency%"
  --let $extra_debug_info= $warning_message
  --source include/assert.inc
  --let $extra_debug_info=
}
--connection server_1

# Check counters.
--let $automatic_counter= query_get_value(SHOW STATUS LIKE "AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT", Value, 1)
--let $anonymous_counter= query_get_value(SHOW STATUS LIKE "ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT", Value, 1)

--connection default

# Compute what counter values to expect. By default, expect both to be 0.
--let $automatic_counter_expected= 0
--let $anonymous_counter_expected= 0
--let $automatic_counter_expected_text= 0
--let $anonymous_counter_expected_text= 0
if ($rpl_debug)
{
  --echo !!!expectation='$expectation' cv='$expect_counter_value' gtid_next='$gtid_next'
}
if ($expectation != 1)
{
  --let $expect_counter_text= $expect_counter_value
  if ($expect_counter_value_row != '')
  {
    --let $expect_counter_text= $expect_counter_value (stm) / $expect_counter_value_row (row)
    if ($binlog_format == 'ROW')
    {
      --let $expect_counter_value= $expect_counter_value_row
    }
  }
  if ($gtid_next == 'AUTOMATIC')
  {
    --let $automatic_counter_expected= $expect_counter_value
    --let $automatic_counter_expected_text= $expect_counter_text
  }
  if ($gtid_next == 'ANONYMOUS')
  {
    --let $anonymous_counter_expected= $expect_counter_value
    --let $anonymous_counter_expected_text= $expect_counter_text
  }
}

--let $assert_text= AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be $automatic_counter_expected_text
--let $assert_cond= $automatic_counter = $automatic_counter_expected
--source include/assert.inc

--let $assert_text= ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be $anonymous_counter_expected_text
--let $assert_cond= $anonymous_counter = $anonymous_counter_expected
--source include/assert.inc

--connection server_1

# Clear transaction state.
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';

# Check that counters are reset.
--let $automatic_counter= query_get_value(SHOW STATUS LIKE "AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT", Value, 1)
--let $anonymous_counter= query_get_value(SHOW STATUS LIKE "ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT", Value, 1)

--let $assert_text= Both counters should be 0
--let $assert_cond= $automatic_counter = 0 AND $anonymous_counter = 0
--source include/assert.inc

--let $expect_counter_value= 0
--let $expect_counter_value_row=
--let $pre_statement=
--let $statements=
