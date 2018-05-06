RESET MASTER;
SET @start_global_value = @@global.gtid_mode;
SELECT @start_global_value;
@start_global_value
OFF
SET @start_enforce_gtid_consistency= @@global.enforce_gtid_consistency;
SET @@global.enforce_gtid_consistency = ON;
select @@global.gtid_mode;
@@global.gtid_mode
OFF
select @@session.gtid_mode;
ERROR HY000: Variable 'gtid_mode' is a GLOBAL variable
set gtid_mode = @start_global_value;
ERROR HY000: Variable 'gtid_mode' is a GLOBAL variable and should be set with SET GLOBAL
set @@session.gtid_mode = @start_global_value;
ERROR HY000: Variable 'gtid_mode' is a GLOBAL variable and should be set with SET GLOBAL
show global variables like 'gtid_mode';
Variable_name	Value
gtid_mode	OFF
show session variables like 'gtid_mode';
Variable_name	Value
gtid_mode	OFF
select * from performance_schema.global_variables where variable_name='gtid_mode';
VARIABLE_NAME	VARIABLE_VALUE
gtid_mode	OFF
select * from performance_schema.session_variables where variable_name='gtid_mode';
VARIABLE_NAME	VARIABLE_VALUE
gtid_mode	OFF
set global gtid_mode = OFF;
set global gtid_mode = ON_PERMISSIVE;
ERROR HY000: The value of @@GLOBAL.GTID_MODE can only be changed one step at a time: OFF <-> OFF_PERMISSIVE <-> ON_PERMISSIVE <-> ON. Also note that this value must be stepped up or down simultaneously on all servers. See the Manual for instructions.
set global gtid_mode = ON;
ERROR HY000: The value of @@GLOBAL.GTID_MODE can only be changed one step at a time: OFF <-> OFF_PERMISSIVE <-> ON_PERMISSIVE <-> ON. Also note that this value must be stepped up or down simultaneously on all servers. See the Manual for instructions.
set global gtid_mode = OFF_PERMISSIVE;
set global gtid_mode = OFF_PERMISSIVE;
set global gtid_mode = ON;
ERROR HY000: The value of @@GLOBAL.GTID_MODE can only be changed one step at a time: OFF <-> OFF_PERMISSIVE <-> ON_PERMISSIVE <-> ON. Also note that this value must be stepped up or down simultaneously on all servers. See the Manual for instructions.
set global gtid_mode = ON_PERMISSIVE;
set global gtid_mode = ON_PERMISSIVE;
set global gtid_mode = OFF;
ERROR HY000: The value of @@GLOBAL.GTID_MODE can only be changed one step at a time: OFF <-> OFF_PERMISSIVE <-> ON_PERMISSIVE <-> ON. Also note that this value must be stepped up or down simultaneously on all servers. See the Manual for instructions.
set global gtid_mode = ON;
set global gtid_mode = ON;
set global gtid_mode = OFF;
ERROR HY000: The value of @@GLOBAL.GTID_MODE can only be changed one step at a time: OFF <-> OFF_PERMISSIVE <-> ON_PERMISSIVE <-> ON. Also note that this value must be stepped up or down simultaneously on all servers. See the Manual for instructions.
set global gtid_mode = OFF_PERMISSIVE;
ERROR HY000: The value of @@GLOBAL.GTID_MODE can only be changed one step at a time: OFF <-> OFF_PERMISSIVE <-> ON_PERMISSIVE <-> ON. Also note that this value must be stepped up or down simultaneously on all servers. See the Manual for instructions.
set global gtid_mode = ON_PERMISSIVE;
set global gtid_mode = OFF_PERMISSIVE;
set global gtid_mode = OFF;
set global gtid_mode = NULL;
ERROR 42000: Variable 'gtid_mode' can't be set to the value of 'NULL'
select @@global.gtid_mode;
@@global.gtid_mode
OFF
set global gtid_mode = default;
select @@global.gtid_mode;
@@global.gtid_mode
OFF
set global gtid_mode = OFF_PERMISSIVE;
set global gtid_mode = default;
select @@global.gtid_mode;
@@global.gtid_mode
OFF
set global gtid_mode = OFF_PERMISSIVE;
set global gtid_mode = ON_PERMISSIVE;
set global gtid_mode = default;
ERROR HY000: The value of @@GLOBAL.GTID_MODE can only be changed one step at a time: OFF <-> OFF_PERMISSIVE <-> ON_PERMISSIVE <-> ON. Also note that this value must be stepped up or down simultaneously on all servers. See the Manual for instructions.
select @@global.gtid_mode;
@@global.gtid_mode
ON_PERMISSIVE
set global gtid_mode = ON;
set global gtid_mode = default;
ERROR HY000: The value of @@GLOBAL.GTID_MODE can only be changed one step at a time: OFF <-> OFF_PERMISSIVE <-> ON_PERMISSIVE <-> ON. Also note that this value must be stepped up or down simultaneously on all servers. See the Manual for instructions.
select @@global.gtid_mode;
@@global.gtid_mode
ON
set session gtid_mode = ON;
ERROR HY000: Variable 'gtid_mode' is a GLOBAL variable and should be set with SET GLOBAL
set global gtid_mode = on_permissive;
set global sql_slave_skip_counter = 1;
set global gtid_mode = on;
ERROR HY000: SET @@GLOBAL.GTID_MODE = ON is not allowed because @@GLOBAL.SQL_SLAVE_SKIP_COUNTER is greater than zero.
set global sql_slave_skip_counter = 0;
set global gtid_mode = on;
set global gtid_mode = on_permissive;
SET @@global.enforce_gtid_consistency = OFF;
set global gtid_mode = off_permissive;
set global gtid_mode = off;
set global gtid_mode = off_permissive;
set global gtid_mode = on_permissive;
set global gtid_mode = on;
ERROR HY000: SET @@GLOBAL.GTID_MODE = ON is not allowed because ENFORCE_GTID_CONSISTENCY is not ON.
SET @@global.enforce_gtid_consistency = WARN;
set global gtid_mode = off_permissive;
set global gtid_mode = off;
set global gtid_mode = off_permissive;
set global gtid_mode = on_permissive;
set global gtid_mode = on;
ERROR HY000: SET @@GLOBAL.GTID_MODE = ON is not allowed because ENFORCE_GTID_CONSISTENCY is not ON.
RESET MASTER;
SET @@GTID_NEXT='11111111-1111-1111-1111-111111111111:1';
BEGIN;
SET @@global.gtid_purged= '11111111-1111-1111-1111-111111111111:1-2';
ERROR HY000: @@GLOBAL.GTID_PURGED cannot be changed: the added gtid set must not overlap with @@GLOBAL.GTID_OWNED
SET @@global.gtid_purged= '11111111-1111-1111-1111-111111111111:2-2';
SET @@global.gtid_purged= '+11111111-1111-1111-1111-111111111111:1-1';
ERROR HY000: @@GLOBAL.GTID_PURGED cannot be changed: the added gtid set must not overlap with @@GLOBAL.GTID_OWNED
SET @@global.gtid_purged= '+11111111-1111-1111-1111-111111111111:3-3';
SELECT @@global.gtid_purged;
@@global.gtid_purged
11111111-1111-1111-1111-111111111111:2-3
ROLLBACK;
CREATE TABLE t1 (a INT);
SET AUTOCOMMIT = 0;
SET SESSION GTID_NEXT = 'aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:1';
INSERT INTO t1 VALUES (1);
SET GLOBAL GTID_MODE = 'off_permissive';
ERROR HY000: The system variable gtid_mode cannot be set when there is an ongoing transaction.
COMMIT;
SET SESSION GTID_NEXT = 'aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:2';
SET GLOBAL GTID_MODE = 'off_permissive';
ERROR HY000: Variable gtid_mode cannot be changed by a client that owns a GTID. The client owns aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:2. Ownership is released on COMMIT or ROLLBACK.
COMMIT;
SET GTID_NEXT = 'ANONYMOUS';
SET GLOBAL GTID_MODE = 'off_permissive';
ERROR HY000: Variable gtid_mode cannot be changed by a client that owns a GTID. The client owns ANONYMOUS. Ownership is released on COMMIT or ROLLBACK.
COMMIT;
SET AUTOCOMMIT = 1;
SET SESSION GTID_NEXT = 'AUTOMATIC';
DROP TABLE t1;
SET GLOBAL GTID_MODE ='off_permissive';
SET GLOBAL GTID_MODE = 'off';
SET @@global.enforce_gtid_consistency = @start_enforce_gtid_consistency;
