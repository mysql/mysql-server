#
# Destructive stored procedure tests
#
--echo #
--echo # Bug#51376 Assert `! is_set()' failed in 
--echo #           Diagnostics_area::set_ok_status on DROP FUNCTION
--echo #

--disable_warnings
DROP FUNCTION IF EXISTS f1;
--enable_warnings

CREATE FUNCTION f1() RETURNS INT RETURN 1;

--echo # Backup the procs_priv table
RENAME TABLE mysql.procs_priv TO mysql.procs_priv_backup;
FLUSH TABLE mysql.procs_priv;

# DROP FUNCTION used to cause an assert.
--error ER_NO_SUCH_TABLE
DROP FUNCTION f1;
SHOW WARNINGS;

--echo # Restore the procs_priv table
RENAME TABLE mysql.procs_priv_backup TO mysql.procs_priv;
FLUSH TABLE mysql.procs_priv;
