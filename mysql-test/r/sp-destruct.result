#
# Bug#51376 Assert `! is_set()' failed in 
#           Diagnostics_area::set_ok_status on DROP FUNCTION
#
DROP FUNCTION IF EXISTS f1;
CREATE FUNCTION f1() RETURNS INT RETURN 1;
# Backup the procs_priv table
RENAME TABLE mysql.procs_priv TO mysql.procs_priv_backup;
FLUSH TABLE mysql.procs_priv;
DROP FUNCTION f1;
ERROR 42S02: Table 'mysql.procs_priv' doesn't exist
SHOW WARNINGS;
Level	Code	Message
Error	1146	Table 'mysql.procs_priv' doesn't exist
Warning	1405	Failed to revoke all privileges to dropped routine
# Restore the procs_priv table
RENAME TABLE mysql.procs_priv_backup TO mysql.procs_priv;
FLUSH TABLE mysql.procs_priv;
