################################################################################
#
# Checks if an error has occurred and if so rolls back the entire transaction.
# Only source this file when such behavior is needed.
#
# Since this file needs to be sourced _after_ the statement that we want to check
# for error, any unacceptable errors will have already caused the test to fail.
# If we get this far, we know that the error was a valid one.
#
# Typical usage in testcase:
# -------------------------------------------------------------------
#  --error 0, ER_LOCK_DEADLOCK, ER_LOCK_WAIT_TIMEOUT
#  UPDATE t1 SET `int1` = `int1` - 4 WHERE `pk` < 25 LIMIT 1;
#  --source suite/stress_tx_rr/include/check_for_error_rollback.inc
# -------------------------------------------------------------------
#
# Examples of "valid" error types in transactional testing:
#  1205 - ER_LOCK_WAIT_TIMEOUT
#  1213 - ER_LOCK_DEADLOCK
#  1020 - ER_CHECKREAD (Falcon: "Record has changed since last read")
#
# In some situations duplicate key errors etc. are also valid.
#
# We keep an approximate count of the number of errors / rollbacks.
# We don't distinguish between error types, as this would require extra queries,
# reducing concurrency.
#
# We do an explicit rollback to make sure all engines have identical behavior on
# transactional errors (some engines only roll back the last statement in some
# cases).
# We don't show this in the result file because we don't know when it will 
# occur and we don't want diffs because of legitimate ROLLBACKs. If, however
# we want to go back and trace ROLLBACKS of this kind, then we need another
# solution.
#
# At this time we skip the rest of the test to avoid rsult file diff problems
# in error situations vs. non-error situations in later parts of the test,
# e.g. repeatable read checking (requires some output to be useful).
#
################################################################################

--disable_query_log

# (Re-) set the error variable in case it has been set to a different value previously.
# This value may be read by the wrapping test script to check if there really
# was an error or not.
let $error= 0;
if ($mysql_errno)
{
    # Last statement sent to the server resulted in an error (0 means no error).

    # Set error variable, because this is used by wrapping tests to determine whether or not
    # to continue with other statements in the same transaction. If set, this indicates that
    # the last statement executed before calling this script resulted in an error.

    let $error= $mysql_errno;

    ## Old code for determining error type...
    #let $deadlock= `SELECT IF($mysql_errno = 1213, 1, 0)`;
    #let $timeout= `SELECT IF($mysql_errno = 1205, 1, 0)`;
    #if ($deadlock) { ... } (etc.)

    # Do a full rollback of the current transaction.
    ROLLBACK;

    # update statistics
    # TODO: Only do this every n times (e.g. n = 10 or 100) to reduce contention.
    # An idea is to use some MOD expression to determine this (e.g. mod of timestamp or conn_id).
    --error 0, ER_LOCK_DEADLOCK, ER_LOCK_WAIT_TIMEOUT, ER_CHECKREAD
    UPDATE statistics SET tx_errors = tx_errors + 1;

    --skip Skip rest of the test due to transactional error (deadlock, timeout, etc.)
}

--enable_query_log

