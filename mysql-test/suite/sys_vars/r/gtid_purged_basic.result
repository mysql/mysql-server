include/gtid_utils.inc
RESET MASTER;
SET @start_global_value = @@global.gtid_purged;
SELECT @start_global_value;
@start_global_value

SET @sql_log_bin_start_value = @@session.sql_log_bin;
select @@global.gtid_purged;
@@global.gtid_purged

select @@session.gtid_purged;
ERROR HY000: Variable 'gtid_purged' is a GLOBAL variable
show global variables like 'gtid_purged';
Variable_name	Value
gtid_purged	
show session variables like 'gtid_purged';
Variable_name	Value
gtid_purged	
select * from performance_schema.global_variables where variable_name='gtid_purged';
VARIABLE_NAME	VARIABLE_VALUE
gtid_purged	
select * from performance_schema.session_variables where variable_name='gtid_purged';
VARIABLE_NAME	VARIABLE_VALUE
gtid_purged	
set @@global.gtid_purged = NULL;
ERROR 42000: Variable 'gtid_purged' can't be set to the value of 'NULL'
select @@global.gtid_purged;
@@global.gtid_purged

set @@global.gtid_purged = default;
ERROR 42000: Variable 'gtid_purged' doesn't have a default value
select @@global.gtid_purged;
@@global.gtid_purged

SET @@global.gtid_purged= '11111111-1111-1111-1111-111111111111:1-2';
SELECT @@global.gtid_purged;
@@global.gtid_purged
11111111-1111-1111-1111-111111111111:1-2
SET @@global.gtid_purged= '11111111-1111-1111-1111-111111111111:3';
ERROR HY000: @@GLOBAL.GTID_PURGED cannot be changed: the new value must be a superset of the old value
SELECT @@global.gtid_purged;
@@global.gtid_purged
11111111-1111-1111-1111-111111111111:1-2
SET @gtid_purged_new='11111111-1111-1111-1111-111111111111:1-2:4-5';
SET @gtid_executed_old= @@GLOBAL.GTID_EXECUTED;
SET @@global.gtid_purged= @gtid_purged_new;
SELECT @@global.gtid_purged;
@@global.gtid_purged
11111111-1111-1111-1111-111111111111:1-2:4-5
include/assert.inc [New value of GTID_EXECUTED must be union of the old one an new purged]
RESET MASTER;
SET AUTOCOMMIT= OFF;
SET @@session.sql_log_bin= @sql_log_bin_start_value;
SET AUTOCOMMIT= ON;
SET @gtid_purged_init= '11111111-1111-1111-1111-111111111111:4-5';
SET @@global.gtid_purged= @gtid_purged_init;
SET @@global.gtid_purged= '+11111111-1111-1111-1111-111111111111:5';
ERROR HY000: @@GLOBAL.GTID_PURGED cannot be changed: the added gtid set must not overlap with @@GLOBAL.GTID_EXECUTED
SET @gtid_executed_old= @@GLOBAL.GTID_EXECUTED;
SET @gtid_purged_incr="11111111-1111-1111-1111-111111111111:6-6";
SET @@global.gtid_purged= CONCAT("+",@gtid_purged_incr);
include/assert.inc [New value of GTID_PURGED must be union of the old one and the increment]
include/assert.inc [New value of GTID_EXECUTED must be union of the old one and the purged increment]
SET @@global.gtid_purged= '+         11111111-1111-1111-1111-111111111111:7';
SET @@global.gtid_purged= '         +11111111-1111-1111-1111-111111111111:8';
SET @@global.gtid_purged= '         -+11111111-1111-1111-1111-111111111111:9';
ERROR 42000: Variable 'gtid_purged' can't be set to the value of '         -+11111111-1111-1111-1111-111111111111:9'
SET @@global.gtid_purged= '         +-11111111-1111-1111-1111-111111111111:9';
ERROR 42000: Variable 'gtid_purged' can't be set to the value of '         +-11111111-1111-1111-1111-111111111111:9'
SET @@global.gtid_purged= '         +,11111111-1111-1111-1111-111111111111:9';
include/assert.inc [Computed purged set is not the same as of the global var]
RESET MASTER;
SELECT @@global.gtid_purged;
@@global.gtid_purged

include/gtid_utils_end.inc
