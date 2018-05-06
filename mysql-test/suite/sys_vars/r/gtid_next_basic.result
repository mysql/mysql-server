SET @start_global_value = @@session.gtid_next;
SELECT @start_global_value;
@start_global_value
AUTOMATIC
select @@global.gtid_next;
ERROR HY000: Variable 'gtid_next' is a SESSION variable
select @@session.gtid_next;
@@session.gtid_next
AUTOMATIC
show global variables like 'gtid_next';
Variable_name	Value
show session variables like 'gtid_next';
Variable_name	Value
gtid_next	AUTOMATIC
select * from performance_schema.global_variables where variable_name='gtid_next';
VARIABLE_NAME	VARIABLE_VALUE
select * from performance_schema.session_variables where variable_name='gtid_next';
VARIABLE_NAME	VARIABLE_VALUE
gtid_next	AUTOMATIC
set global gtid_next = 1;
ERROR HY000: Variable 'gtid_next' is a SESSION variable and can't be used with SET GLOBAL
set session gtid_next = "ANONYMOUS";
select @@session.gtid_next;
@@session.gtid_next
ANONYMOUS
ROLLBACK;
set session gtid_next=default;
select @@session.gtid_next;
@@session.gtid_next
AUTOMATIC
ROLLBACK;
set session gtid_next=NULL;
ERROR 42000: Variable 'gtid_next' can't be set to the value of 'NULL'
select @@session.gtid_next;
@@session.gtid_next
AUTOMATIC
SET @@SESSION.GTID_NEXT = 'ANONYMOUS';
SET @@SESSION.GTID_NEXT = default;
ERROR HY000: @@SESSION.GTID_NEXT cannot be changed by a client that owns a GTID. The client owns ANONYMOUS. Ownership is released on COMMIT or ROLLBACK.
SELECT @@SESSION.GTID_NEXT;
@@SESSION.GTID_NEXT
ANONYMOUS
ROLLBACK;
set session gtid_next = @start_global_value;
select @@session.gtid_next;
@@session.gtid_next
AUTOMATIC
