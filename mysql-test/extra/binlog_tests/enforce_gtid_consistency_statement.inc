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
# --let $statement_ends_transaction= [0|1]
# [--let $statement_ends_transaction_row= [0|1]]
# --let $violation_result= [0|1|2]
# --let $error_code= ER_SOMETHING
# --let $error_message= some text
# --let $sync_point= DEBUG_SYNC_POINT
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
#   $statement_ends_transaction
#     Set this to 0 if the transaction is still expected to be open
#     after $statement has been executed; set it to 1 if the
#     transaction is expected to have ended. This affects the expected
#     value of the counters
#     ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT and
#     ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT, since a
#     transaction that increases the counters will decrease them when
#     the transaction ends.
#
#   $statement_ends_transaction_row
#     Normally, $statement_ends_transaction is used regardless of
#     BINLOG_FORMAT.  But in some corner cases, the value of
#     $statement_ends_transaction depends on BINLOG_FORMAT.  For those
#     cases, the caller should set $statement_ends_transaction to the
#     expected value when BINLOG_FORMAT==STATEMENT or MIXED, and
#     $statement_ends_transaction_row to the expected value when
#     BINLOG_FORMAT=ROW.  The output to the result file will be the
#     same regardless of BINLOG_FORMAT.
#
#   $error_code
#     The expected error code (ER_*), in case $statement generates an
#     error.
#
#   $error_message
#     A substring of the error message text, in case $statement
#     generates an error.

--echo # enforce_gtid_consistency=$enforce_gtid_consistency gtid_mode=$gtid_mode gtid_next=$gtid_next

if ($statement_ends_transaction == '')
{
  --die !!!ERROR in test: set $statement_ends_transaction before sourcing enforce_gtid_consistency_statement.inc
}
if ($extra_warning_count == '')
{
  --let $extra_warning_count= 0
}

# $expected_status is 0 if the statement is ok, 1 if an error is expected,
# 2 if a warning is expected.
#
# $expected_counter is 1 if any of the counters
# ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT or
# ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT is expected to
# increase, 0 otherwise.
--let $expected_status= 0
--let $expected_counter= 0
if ($gtid_violation)
{
  --let $expected_status= $violation_result
  if ($violation_result != 1)
  {
    --let $expected_counter= 1
  }
}

if ($rpl_debug)
{
  --echo gtid_violation='$gtid_violation' violation_result='$violation_result' error_code='$error_code' expectation='$expected_status' statement_ends_transaction='$statement_ends_transaction' statement_ends_transaction_row='$statement_ends_transaction_row'
  --echo pre_statement='$pre_statement'
  --echo gtid_next='$gtid_next'
  --echo statement='$statement'
}

--connection $statement_connection
--let $binlog_format= `SELECT @@GLOBAL.BINLOG_FORMAT`
--source include/set_gtid_next_gtid_mode_agnostic.inc

if ($pre_statement != '')
{
  eval $pre_statement;
}

--let $automatic_counter_expected= 0
--let $anonymous_counter_expected= 0

if ($expected_status == 1)
{
  --echo error $error_code
  --replace_regex /[a-f0-9]{8}-[a-f0-9]{4}-[a-f0-9]{4}-[a-f0-9]{4}-[a-f0-9]{12}:[0-9]*/#/
  --error $error_code
  eval $statement;
}
if ($expected_status != 1)
{
  --source include/execute_to_sync_point.inc
  --connection $auxiliary_connection

  if ($expected_counter)
  {
    if ($gtid_next == 'AUTOMATIC')
    {
      --let $automatic_counter_expected= 1
    }
    if ($gtid_next == 'ANONYMOUS')
    {
      --let $anonymous_counter_expected= 1
    }
  }

  --let $automatic_counter= query_get_value(SHOW STATUS LIKE "ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT", Value, 1)
  --let $anonymous_counter= query_get_value(SHOW STATUS LIKE "ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT", Value, 1)

  --let $assert_text= ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be $automatic_counter_expected
  --let $assert_cond= $automatic_counter = $automatic_counter_expected
  --source include/assert.inc

  --let $assert_text= ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be $anonymous_counter_expected
  --let $assert_cond= $anonymous_counter = $anonymous_counter_expected
  --source include/assert.inc

  --source include/execute_from_sync_point.inc
}

# Check warnings / errors.

--connection $statement_connection
--let $warning_count= `SHOW COUNT(*) WARNINGS`
--let $warning_message= query_get_value(SHOW WARNINGS, Message, 1)
--let $warning_level= query_get_value(SHOW WARNINGS, Level, 1)

--connection $auxiliary_connection

# The message may contain a single quote ('), which confuses
# include/assert.inc So we remove any single quotes.
--let $warning_message= `SELECT REPLACE("$warning_message", "'", "")`

if ($expected_status == 0)
{
  --let $assert_text= No warning or error should be generated.
  --let $assert_cond= $warning_count = 0 + $extra_warning_count
  --let $extra_debug_info= $warning_message
  --source include/assert.inc
  --let $extra_debug_info=
}
if ($expected_status == 1)
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
if ($expected_status == 2)
{
  --let $assert_text= One warning/error should be generated.
  --let $assert_cond= $warning_count = 1 + $extra_warning_count
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
#--connection server_1

# Check counters.
--let $automatic_counter= query_get_value(SHOW STATUS LIKE "ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT", Value, 1)
--let $anonymous_counter= query_get_value(SHOW STATUS LIKE "ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT", Value, 1)

#--connection default

# Compute what counter values to expect. By default, expect both to be 0.
if ($rpl_debug)
{
  --echo !!!expectation='$expected_status' cv='$expect_counter_value' gtid_next='$gtid_next'
}

--let $expect_end_transaction= $statement_ends_transaction
if ($statement_ends_transaction_row != '')
{
  if ($binlog_format == 'ROW')
  {
    --let $expect_end_transaction= $statement_ends_transaction_row
  }
  --let $counter_expected_text= $statement_ends_transaction_row (stm) / $statement_ends_transaction (row)
  if ($gtid_next == 'AUTOMATIC')
  {
    --let $automatic_counter_expected_text= $counter_expected_text
  }
  if ($gtid_next == 'ANONYMOUS')
  {
    --let $anonymous_counter_expected_text= $counter_expected_text
  }
}
if ($expect_end_transaction)
{
  --let $automatic_counter_expected= 0
  --let $anonymous_counter_expected= 0
}
if ($statement_ends_transaction_row == '')
{
  --let $automatic_counter_expected_text= $automatic_counter_expected
  --let $anonymous_counter_expected_text= $anonymous_counter_expected
}

--let $assert_text= ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be $automatic_counter_expected_text
--let $assert_cond= $automatic_counter = $automatic_counter_expected
--source include/assert.inc

--let $assert_text= ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be $anonymous_counter_expected_text
--let $assert_cond= $anonymous_counter = $anonymous_counter_expected
--source include/assert.inc

--connection $statement_connection

# Clear transaction state.
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';

# Check that counters are reset.
--let $automatic_counter= query_get_value(SHOW STATUS LIKE "ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT", Value, 1)
--let $anonymous_counter= query_get_value(SHOW STATUS LIKE "ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT", Value, 1)

--let $assert_text= Both counters should be 0
--let $assert_cond= $automatic_counter = 0 AND $anonymous_counter = 0
--source include/assert.inc

#--let $statement_ends_transaction= 0
--let $statement_ends_transaction_row=
--let $pre_statement=
