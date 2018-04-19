RESET MASTER;
SET @start_global_value = @@global.enforce_gtid_consistency;
SELECT @start_global_value;
@start_global_value
OFF
select @@global.enforce_gtid_consistency;
@@global.enforce_gtid_consistency
OFF
select @@session.enforce_gtid_consistency;
ERROR HY000: Variable 'enforce_gtid_consistency' is a GLOBAL variable
show global variables like 'enforce_gtid_consistency';
Variable_name	Value
enforce_gtid_consistency	OFF
show session variables like 'enforce_gtid_consistency';
Variable_name	Value
enforce_gtid_consistency	OFF
select * from performance_schema.global_variables where variable_name='enforce_gtid_consistency';
VARIABLE_NAME	VARIABLE_VALUE
enforce_gtid_consistency	OFF
select * from performance_schema.session_variables where variable_name='enforce_gtid_consistency';
VARIABLE_NAME	VARIABLE_VALUE
enforce_gtid_consistency	OFF
set global enforce_gtid_consistency= 1;
select @@global.enforce_gtid_consistency;
@@global.enforce_gtid_consistency
ON
show variables like 'enforce_gtid_consistency';
Variable_name	Value
enforce_gtid_consistency	ON
select variable_value from performance_schema.global_variables
where variable_name= 'enforce_gtid_consistency';
variable_value
ON
set global enforce_gtid_consistency= 0;
select @@global.enforce_gtid_consistency;
@@global.enforce_gtid_consistency
OFF
show variables like 'enforce_gtid_consistency';
Variable_name	Value
enforce_gtid_consistency	OFF
select variable_value from performance_schema.global_variables
where variable_name= 'enforce_gtid_consistency';
variable_value
OFF
set global enforce_gtid_consistency= 2;
select @@global.enforce_gtid_consistency;
@@global.enforce_gtid_consistency
WARN
show variables like 'enforce_gtid_consistency';
Variable_name	Value
enforce_gtid_consistency	WARN
select variable_value from performance_schema.global_variables
where variable_name= 'enforce_gtid_consistency';
variable_value
WARN
set global enforce_gtid_consistency= ON;
select @@global.enforce_gtid_consistency;
@@global.enforce_gtid_consistency
ON
show variables like 'enforce_gtid_consistency';
Variable_name	Value
enforce_gtid_consistency	ON
select variable_value from performance_schema.global_variables
where variable_name= 'enforce_gtid_consistency';
variable_value
ON
set global enforce_gtid_consistency= OFF;
select @@global.enforce_gtid_consistency;
@@global.enforce_gtid_consistency
OFF
show variables like 'enforce_gtid_consistency';
Variable_name	Value
enforce_gtid_consistency	OFF
select variable_value from performance_schema.global_variables
where variable_name= 'enforce_gtid_consistency';
variable_value
OFF
set global enforce_gtid_consistency= TRUE;
select @@global.enforce_gtid_consistency;
@@global.enforce_gtid_consistency
ON
show variables like 'enforce_gtid_consistency';
Variable_name	Value
enforce_gtid_consistency	ON
select variable_value from performance_schema.global_variables
where variable_name= 'enforce_gtid_consistency';
variable_value
ON
set global enforce_gtid_consistency= FALSE;
select @@global.enforce_gtid_consistency;
@@global.enforce_gtid_consistency
OFF
show variables like 'enforce_gtid_consistency';
Variable_name	Value
enforce_gtid_consistency	OFF
select variable_value from performance_schema.global_variables
where variable_name= 'enforce_gtid_consistency';
variable_value
OFF
set global enforce_gtid_consistency= WARN;
select @@global.enforce_gtid_consistency;
@@global.enforce_gtid_consistency
WARN
show variables like 'enforce_gtid_consistency';
Variable_name	Value
enforce_gtid_consistency	WARN
select variable_value from performance_schema.global_variables
where variable_name= 'enforce_gtid_consistency';
variable_value
WARN
set session enforce_gtid_consistency= 1;
ERROR HY000: Variable 'enforce_gtid_consistency' is a GLOBAL variable and should be set with SET GLOBAL
select @@session.enforce_gtid_consistency;
ERROR HY000: Variable 'enforce_gtid_consistency' is a GLOBAL variable
set global enforce_gtid_consistency= -1;
ERROR 42000: Variable 'enforce_gtid_consistency' can't be set to the value of '-1'
set global enforce_gtid_consistency= 3;
ERROR 42000: Variable 'enforce_gtid_consistency' can't be set to the value of '3'
set global enforce_gtid_consistency= NULL;
ERROR 42000: Variable 'enforce_gtid_consistency' can't be set to the value of 'NULL'
set global enforce_gtid_consistency= '';
ERROR 42000: Variable 'enforce_gtid_consistency' can't be set to the value of ''
set global enforce_gtid_consistency= default;
set global enforce_gtid_consistency= ON;
set global gtid_mode=off_permissive;
set global gtid_mode=on_permissive;
set global gtid_mode=on;
set global enforce_gtid_consistency= WARN;
ERROR HY000: GTID_MODE = ON requires ENFORCE_GTID_CONSISTENCY = ON.
set global enforce_gtid_consistency= OFF;
ERROR HY000: GTID_MODE = ON requires ENFORCE_GTID_CONSISTENCY = ON.
set global enforce_gtid_consistency= ON;
set global enforce_gtid_consistency= TRUE;
set global enforce_gtid_consistency= 1;
set global gtid_mode=on_permissive;
set global gtid_mode=off_permissive;
CREATE TABLE t1 (a INT);
SET AUTOCOMMIT = 0;
SET SESSION GTID_NEXT = 'aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:1';
INSERT INTO t1 VALUES (1);
SET GLOBAL ENFORCE_GTID_CONSISTENCY = 'off';
ERROR HY000: The system variable enforce_gtid_consistency cannot be set when there is an ongoing transaction.
COMMIT;
SET SESSION GTID_NEXT = 'aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:2';
SET GLOBAL ENFORCE_GTID_CONSISTENCY = 'off';
ERROR HY000: Variable enforce_gtid_consistency cannot be changed by a client that owns a GTID. The client owns aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:2. Ownership is released on COMMIT or ROLLBACK.
COMMIT;
SET GTID_NEXT = 'ANONYMOUS';
SET GLOBAL ENFORCE_GTID_CONSISTENCY = 'off';
ERROR HY000: Variable enforce_gtid_consistency cannot be changed by a client that owns a GTID. The client owns ANONYMOUS. Ownership is released on COMMIT or ROLLBACK.
COMMIT;
SET AUTOCOMMIT = 1;
SET SESSION GTID_NEXT = 'AUTOMATIC';
SET GLOBAL GTID_MODE = 'off';
DROP TABLE t1;
set global enforce_gtid_consistency= @start_global_value;
