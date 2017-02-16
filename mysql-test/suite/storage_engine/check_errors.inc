#
# Since we run tests in disable_abort_on_error mode, we cannot use --error command,
# and need to check the result manually.
# Usage in a test:
# --let $error_codes = <comma-separated list>  # optional, default ''
# --let $mysql_errname = <error name>          # optional, default current $mysql_errname (from the last SQL command)
# --let $mysql_errno = <error code>            # optional, default current $mysql_errno (from the last SQL command)
#

if ($error_codes == '0')
{
  --let $error_codes =
}
if ($error_codes == '')
{
  if ($mysql_errname)
  {
    --echo # ERROR: Statement ended with errno $mysql_errno, errname $mysql_errname (expected to succeed)
  }

  # If both error_codes and mysql_errname are false, all is good, no logic needed
}

if ($error_codes != '')
{
  # If mysql_errname or mysql_errno is equal to $error_codes, it's good too, nothing to do

  if ($mysql_errname != $error_codes)
  {
    if ($mysql_errno != $error_codes)
    {
      --let $save_errno = $mysql_errno
      --let $save_errname = $mysql_errname

      --let $codeline = `SELECT CONCAT('\'',REPLACE('$error_codes',',','\',\''),'\'')`
      --let $result = `SELECT '$save_errname' IN($codeline) or '$save_errno' IN ($codeline)`  

      --let $mysql_errno = $save_errno
      --let $mysql_errname = $save_errname

      if (!$result)
      {
        if ($mysql_errname)
        {
          --echo # ERROR: Statement ended with errno $mysql_errno, errname $mysql_errname (expected results: $error_codes)
        }
        if (!$mysql_errname)
        {
          --echo # ERROR: Statement succeeded (expected results: $error_codes)
        }
      }
      # If a list contained more than one error, it could be on one of two reasons:
      # first, we do not care which code it is, as long as it is one of the listed errors.
      # In this case we will suggest to add an rdiff file if the message differs.
      # Second, check_errors might be called from a generalized include file or test, 
      # which runs with different parameters and thus might produce different results for the same statement.
      # Then, the message will be stricter, as the difference with the result file is actually a problem
      # which needs to be checked at least.
      if ($result)
      {
        if (!$strict_check)
        {
          --echo # Statement ended with one of expected results ($error_codes). 
          --echo # If you got a difference in error message, just add it to rdiff file
        }
        if ($strict_check)
        {
          --echo # WARNING: Statement ended with errno $mysql_errno, errname '$mysql_errname'. 
          --echo #          If it differs from the result file, it might indicate a problem.
        }
        
      }
    }
  }
}

# Don't want the variables to be accidentally reused later
--let $error_codes =
--let $strict_check =

