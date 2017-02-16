#
# This include file prints a standard message about possibly unsupported functionality
# when a statement finished not as expected.
# Usage:
# --let $functionality = Foreign keys              # optional
# --let $my_last_stmt = INSERT INTO t VALUES (1)   # optional

--let $versions =

if (!$functionality)
{
  --let $functionality = Functionality
}
if ($mysql_errname)
{
  --let $result = finished with $mysql_errname
  if ($mysql_errname == ER_SYNTAX_ERROR)
  {
    --let $functionality = $functionality or the syntax
  }
  if ($mysql_errname == ER_ILLEGAL_HA)
  {
    --let $functionality = $functionality or the syntax
    --let $versions = unsupported
  }
}
if (!$mysql_errname)
{
  --let $result = succeeded unexpectedly
}
if (!$versions)
{
  --let $versions = unsupported|malfunctioning, or the problem was caused by previous errors
}

--echo # ------------ UNEXPECTED RESULT ------------
if ($my_last_stmt)
{
  --echo # [ $my_last_stmt ]
}
--echo # The statement|command $result.
--echo # $functionality or the mix could be $versions. 
--echo # You can change the engine code, or create an rdiff, or disable the test by adding it to disabled.def.
--echo # Further in this test, the message might sometimes be suppressed; a part of the test might be skipped.
--echo # Also, this problem may cause a chain effect (more errors of different kinds in the test).
--echo # -------------------------------------------

--let $my_last_stmt =
--let $functionality =

