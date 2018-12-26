#
# Initialization
#
INSTALL PLUGIN rpl_semi_sync_master SONAME 'semisync_master.so';
#
# Verify it can set selected and showed correctly
#
SELECT @@GLOBAL.rpl_semi_sync_master_wait_point;
@@GLOBAL.rpl_semi_sync_master_wait_point
AFTER_SYNC
SHOW GLOBAL VARIABLES LIKE 'rpl_semi_sync_master_wait_point';
Variable_name	Value
rpl_semi_sync_master_wait_point	AFTER_SYNC
SELECT * FROM performance_schema.global_variables WHERE
VARIABLE_NAME = 'rpl_semi_sync_master_wait_point';
VARIABLE_NAME	VARIABLE_VALUE
rpl_semi_sync_master_wait_point	AFTER_SYNC
#
# Verify it is not a session variable
#
SELECT @@SESSION.rpl_semi_sync_master_wait_point;
ERROR HY000: Variable 'rpl_semi_sync_master_wait_point' is a GLOBAL variable
SHOW SESSION VARIABLES LIKE 'rpl_semi_sync_master_wait_point';
Variable_name	Value
rpl_semi_sync_master_wait_point	AFTER_SYNC
SELECT * FROM performance_schema.session_variables WHERE
VARIABLE_NAME ='rpl_semi_sync_master_wait_point';
VARIABLE_NAME	VARIABLE_VALUE
rpl_semi_sync_master_wait_point	AFTER_SYNC
#
# Verify it can be set correctly
#
SET GLOBAL rpl_semi_sync_master_wait_point = AFTER_COMMIT;
SELECT @@GLOBAL.rpl_semi_sync_master_wait_point;
@@GLOBAL.rpl_semi_sync_master_wait_point
AFTER_COMMIT
SET GLOBAL rpl_semi_sync_master_wait_point = AFTER_SYNC;
SELECT @@GLOBAL.rpl_semi_sync_master_wait_point;
@@GLOBAL.rpl_semi_sync_master_wait_point
AFTER_SYNC
SET GLOBAL rpl_semi_sync_master_wait_point = 1;
SELECT @@GLOBAL.rpl_semi_sync_master_wait_point;
@@GLOBAL.rpl_semi_sync_master_wait_point
AFTER_COMMIT
SET GLOBAL rpl_semi_sync_master_wait_point = 0;
SELECT @@GLOBAL.rpl_semi_sync_master_wait_point;
@@GLOBAL.rpl_semi_sync_master_wait_point
AFTER_SYNC
SET SESSION rpl_semi_sync_master_wait_point = AFTER_COMMIT;
ERROR HY000: Variable 'rpl_semi_sync_master_wait_point' is a GLOBAL variable and should be set with SET GLOBAL
SHOW SESSION VARIABLES LIKE 'rpl_semi_sync_master_wait_point';
Variable_name	Value
rpl_semi_sync_master_wait_point	AFTER_SYNC
#
# Verify it could not bet set with invalid values
#
SET GLOBAL rpl_semi_sync_master_wait_point = 2;
ERROR 42000: Variable 'rpl_semi_sync_master_wait_point' can't be set to the value of '2'
SET GLOBAL rpl_semi_sync_master_wait_point = blabla;
ERROR 42000: Variable 'rpl_semi_sync_master_wait_point' can't be set to the value of 'blabla'
SET GLOBAL rpl_semi_sync_master_wait_point = "blabla";
ERROR 42000: Variable 'rpl_semi_sync_master_wait_point' can't be set to the value of 'blabla'
SET GLOBAL rpl_semi_sync_master_wait_point = "";
ERROR 42000: Variable 'rpl_semi_sync_master_wait_point' can't be set to the value of ''
SET GLOBAL rpl_semi_sync_master_wait_point = NULL;
ERROR 42000: Variable 'rpl_semi_sync_master_wait_point' can't be set to the value of 'NULL'
SET GLOBAL rpl_semi_sync_master_wait_point = 0.1;
ERROR 42000: Incorrect argument type to variable 'rpl_semi_sync_master_wait_point'
#
# Cleanup
#
UNINSTALL PLUGIN rpl_semi_sync_master;
