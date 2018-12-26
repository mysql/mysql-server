--source include/windows.inc


--echo #
--echo # Bug #23747899: @@basedir sysvar value not normalized if set through
--echo #  the command line/ini file

--echo # There should be no slashes in @@basedir and just backslashes
--echo #   since it's normalized
SELECT
  LOCATE('/', @@basedir) <> 0 AS have_slashes,
  LOCATE('\\', @@basedir) <> 0 AS have_backslashes;


--echo End of the 5.6 tests
