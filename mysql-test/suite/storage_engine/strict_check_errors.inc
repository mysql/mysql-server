#
# Since we run tests in disable_abort_on_error mode, we cannot use --error command,
# and need to check the result manually.
# Usage in a test:
# --let $error_codes = <comma-separated list>  # optional, default ''
# --let $mysql_errname = <error name>          # optional, default current $mysql_errname (from the last SQL command)
# --let $mysql_errno = <error code>            # optional, default current $mysql_errno (from the last SQL command)
#
# The difference between strict_check_errors.inc and check_errors.inc is only 
# that they will produce different messages when there is a list of acceptable error codes (more than one),
# and the statement finished with one of them.

# If a list contained more than one error, it could be on one of two reasons:
# first, we do not care which code it is, as long as it is one of the listed errors.
# In this case we will suggest to add an rdiff file if the message differs.
# Second, check_errors might be called from a generalized include file or test, 
# which runs with different parameters and thus might produce different results for the same statement.
# Then, the message will be stricter, as the difference with the result file is actually a problem
# which needs to be checked at least.

--let $strict_check = 1
--source check_errors.inc

