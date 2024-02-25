#
# PART 1: syntax
#
#
# Test every new reserved and non reserved keywords
#
drop table if exists signal_non_reserved;
create table signal_non_reserved (
class_origin int,
subclass_origin int,
constraint_catalog int,
constraint_schema int,
constraint_name int,
catalog_name int,
schema_name int,
table_name int,
column_name int,
cursor_name int,
message_text int,
sqlcode int
);
drop table signal_non_reserved;
drop table if exists diag_non_reserved;
create table diag_non_reserved (
diagnostics int,
current int,
stacked int,
exception int
);
drop table diag_non_reserved;
drop table if exists diag_cond_non_reserved;
create table diag_cond_non_reserved (
condition_identifier int,
condition_number int,
condition_name int,
connection_name int,
message_length int,
message_octet_length int,
parameter_mode int,
parameter_name int,
parameter_ordinal_position int,
returned_sqlstate int,
routine_catalog int,
routine_name int,
routine_schema int,
server_name int,
specific_name int,
trigger_catalog int,
trigger_name int,
trigger_schema int
);
drop table diag_cond_non_reserved;
drop table if exists diag_stmt_non_reserved;
create table diag_stmt_non_reserved (
number int,
more int,
command_function int,
command_function_code int,
dynamic_function int,
dynamic_function_code int,
row_count int,
transactions_committed int,
transactions_rolled_back int,
transaction_active int
);
drop table diag_stmt_non_reserved;
drop table if exists test_reserved;
create table test_reserved (signal int);
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'signal int)' at line 1
create table test_reserved (resignal int);
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'resignal int)' at line 1
create table test_reserved (condition int);
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'condition int)' at line 1
#
# Test the SIGNAL syntax
#
drop procedure if exists test_invalid;
drop procedure if exists test_signal_syntax;
drop function if exists test_signal_func;
create procedure test_invalid()
begin
SIGNAL;
end $$
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near ';
end' at line 3
create procedure test_invalid()
begin
SIGNAL foo;
end $$
ERROR 42000: Undefined CONDITION: foo
create procedure test_invalid()
begin
DECLARE foo CONDITION FOR 1234;
SIGNAL foo;
end $$
ERROR HY000: SIGNAL/RESIGNAL can only use a CONDITION defined with SQLSTATE
create procedure test_signal_syntax()
begin
DECLARE foo CONDITION FOR SQLSTATE '12345';
SIGNAL foo;
end $$
drop procedure test_signal_syntax $$
create procedure test_signal_syntax()
begin
SIGNAL SQLSTATE '23000';
end $$
drop procedure test_signal_syntax $$
create procedure test_signal_syntax()
begin
SIGNAL SQLSTATE VALUE '23000';
end $$
drop procedure test_signal_syntax $$
create procedure test_signal_syntax()
begin
DECLARE foo CONDITION FOR SQLSTATE '12345';
SIGNAL foo SET CLASS_ORIGIN = 'foo';
end $$
drop procedure test_signal_syntax $$
create procedure test_signal_syntax()
begin
DECLARE foo CONDITION FOR SQLSTATE '12345';
SIGNAL foo SET SUBCLASS_ORIGIN = 'foo';
end $$
drop procedure test_signal_syntax $$
create procedure test_signal_syntax()
begin
DECLARE foo CONDITION FOR SQLSTATE '12345';
SIGNAL foo SET CONSTRAINT_CATALOG = 'foo';
end $$
drop procedure test_signal_syntax $$
create procedure test_signal_syntax()
begin
DECLARE foo CONDITION FOR SQLSTATE '12345';
SIGNAL foo SET CONSTRAINT_SCHEMA = 'foo';
end $$
drop procedure test_signal_syntax $$
create procedure test_signal_syntax()
begin
DECLARE foo CONDITION FOR SQLSTATE '12345';
SIGNAL foo SET CONSTRAINT_NAME = 'foo';
end $$
drop procedure test_signal_syntax $$
create procedure test_signal_syntax()
begin
DECLARE foo CONDITION FOR SQLSTATE '12345';
SIGNAL foo SET CATALOG_NAME = 'foo';
end $$
drop procedure test_signal_syntax $$
create procedure test_signal_syntax()
begin
DECLARE foo CONDITION FOR SQLSTATE '12345';
SIGNAL foo SET SCHEMA_NAME = 'foo';
end $$
drop procedure test_signal_syntax $$
create procedure test_signal_syntax()
begin
DECLARE foo CONDITION FOR SQLSTATE '12345';
SIGNAL foo SET TABLE_NAME = 'foo';
end $$
drop procedure test_signal_syntax $$
create procedure test_signal_syntax()
begin
DECLARE foo CONDITION FOR SQLSTATE '12345';
SIGNAL foo SET COLUMN_NAME = 'foo';
end $$
drop procedure test_signal_syntax $$
create procedure test_signal_syntax()
begin
DECLARE foo CONDITION FOR SQLSTATE '12345';
SIGNAL foo SET CURSOR_NAME = 'foo';
end $$
drop procedure test_signal_syntax $$
create procedure test_signal_syntax()
begin
DECLARE foo CONDITION FOR SQLSTATE '12345';
SIGNAL foo SET MESSAGE_TEXT = 'foo';
end $$
drop procedure test_signal_syntax $$
create procedure test_signal_syntax()
begin
DECLARE foo CONDITION FOR SQLSTATE '12345';
SIGNAL foo SET MYSQL_ERRNO = 'foo';
end $$
drop procedure test_signal_syntax $$
create procedure test_invalid()
begin
DECLARE foo CONDITION FOR SQLSTATE '12345';
SIGNAL foo SET CLASS_ORIGIN = 'foo', CLASS_ORIGIN = 'bar';
end $$
ERROR 42000: Duplicate condition information item 'CLASS_ORIGIN'
create procedure test_invalid()
begin
DECLARE foo CONDITION FOR SQLSTATE '12345';
SIGNAL foo SET MESSAGE_TEXT = 'foo', MESSAGE_TEXT = 'bar';
end $$
ERROR 42000: Duplicate condition information item 'MESSAGE_TEXT'
create procedure test_invalid()
begin
DECLARE foo CONDITION FOR SQLSTATE '12345';
SIGNAL foo SET MYSQL_ERRNO = 'foo', MYSQL_ERRNO = 'bar';
end $$
ERROR 42000: Duplicate condition information item 'MYSQL_ERRNO'
create procedure test_signal_syntax()
begin
DECLARE foo CONDITION FOR SQLSTATE '12345';
SIGNAL foo SET
CLASS_ORIGIN = 'foo',
SUBCLASS_ORIGIN = 'foo',
CONSTRAINT_CATALOG = 'foo',
CONSTRAINT_SCHEMA = 'foo',
CONSTRAINT_NAME = 'foo',
CATALOG_NAME = 'foo',
SCHEMA_NAME = 'foo',
TABLE_NAME = 'foo',
COLUMN_NAME = 'foo',
CURSOR_NAME = 'foo',
MESSAGE_TEXT = 'foo',
MYSQL_ERRNO = 'foo';
end $$
drop procedure test_signal_syntax $$
SIGNAL SQLSTATE '00000' $$
ERROR 42000: Bad SQLSTATE: '00000'
SIGNAL SQLSTATE '00001' $$
ERROR 42000: Bad SQLSTATE: '00001'
create procedure test_invalid()
begin
SIGNAL SQLSTATE '00000';
end $$
ERROR 42000: Bad SQLSTATE: '00000'
create procedure test_invalid()
begin
SIGNAL SQLSTATE '00001';
end $$
ERROR 42000: Bad SQLSTATE: '00001'
#
# Test conditions information that SIGNAL can not set
#
create procedure test_invalid()
begin
SIGNAL SQLSTATE '12345' SET bla_bla = 'foo';
end $$
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'bla_bla = 'foo';
end' at line 3
create procedure test_invalid()
begin
SIGNAL SQLSTATE '12345' SET CONDITION_IDENTIFIER = 'foo';
end $$
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'CONDITION_IDENTIFIER = 'foo';
end' at line 3
create procedure test_invalid()
begin
SIGNAL SQLSTATE '12345' SET CONDITION_NUMBER = 'foo';
end $$
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'CONDITION_NUMBER = 'foo';
end' at line 3
create procedure test_invalid()
begin
SIGNAL SQLSTATE '12345' SET CONNECTION_NAME = 'foo';
end $$
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'CONNECTION_NAME = 'foo';
end' at line 3
create procedure test_invalid()
begin
SIGNAL SQLSTATE '12345' SET MESSAGE_LENGTH = 'foo';
end $$
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'MESSAGE_LENGTH = 'foo';
end' at line 3
create procedure test_invalid()
begin
SIGNAL SQLSTATE '12345' SET MESSAGE_OCTET_LENGTH = 'foo';
end $$
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'MESSAGE_OCTET_LENGTH = 'foo';
end' at line 3
create procedure test_invalid()
begin
SIGNAL SQLSTATE '12345' SET PARAMETER_MODE = 'foo';
end $$
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'PARAMETER_MODE = 'foo';
end' at line 3
create procedure test_invalid()
begin
SIGNAL SQLSTATE '12345' SET PARAMETER_NAME = 'foo';
end $$
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'PARAMETER_NAME = 'foo';
end' at line 3
create procedure test_invalid()
begin
SIGNAL SQLSTATE '12345' SET PARAMETER_ORDINAL_POSITION = 'foo';
end $$
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'PARAMETER_ORDINAL_POSITION = 'foo';
end' at line 3
create procedure test_invalid()
begin
SIGNAL SQLSTATE '12345' SET RETURNED_SQLSTATE = 'foo';
end $$
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'RETURNED_SQLSTATE = 'foo';
end' at line 3
create procedure test_invalid()
begin
SIGNAL SQLSTATE '12345' SET ROUTINE_CATALOG = 'foo';
end $$
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'ROUTINE_CATALOG = 'foo';
end' at line 3
create procedure test_invalid()
begin
SIGNAL SQLSTATE '12345' SET ROUTINE_NAME = 'foo';
end $$
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'ROUTINE_NAME = 'foo';
end' at line 3
create procedure test_invalid()
begin
SIGNAL SQLSTATE '12345' SET ROUTINE_SCHEMA = 'foo';
end $$
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'ROUTINE_SCHEMA = 'foo';
end' at line 3
create procedure test_invalid()
begin
SIGNAL SQLSTATE '12345' SET SERVER_NAME = 'foo';
end $$
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'SERVER_NAME = 'foo';
end' at line 3
create procedure test_invalid()
begin
SIGNAL SQLSTATE '12345' SET SPECIFIC_NAME = 'foo';
end $$
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'SPECIFIC_NAME = 'foo';
end' at line 3
create procedure test_invalid()
begin
SIGNAL SQLSTATE '12345' SET TRIGGER_CATALOG = 'foo';
end $$
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'TRIGGER_CATALOG = 'foo';
end' at line 3
create procedure test_invalid()
begin
SIGNAL SQLSTATE '12345' SET TRIGGER_NAME = 'foo';
end $$
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'TRIGGER_NAME = 'foo';
end' at line 3
create procedure test_invalid()
begin
SIGNAL SQLSTATE '12345' SET TRIGGER_SCHEMA = 'foo';
end $$
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'TRIGGER_SCHEMA = 'foo';
end' at line 3
#
# Test the RESIGNAL syntax
#
drop procedure if exists test_invalid;
drop procedure if exists test_resignal_syntax;
create procedure test_invalid()
begin
RESIGNAL foo;
end $$
ERROR 42000: Undefined CONDITION: foo
create procedure test_resignal_syntax()
begin
RESIGNAL;
end $$
drop procedure test_resignal_syntax $$
create procedure test_invalid()
begin
DECLARE foo CONDITION FOR 1234;
RESIGNAL foo;
end $$
ERROR HY000: SIGNAL/RESIGNAL can only use a CONDITION defined with SQLSTATE
create procedure test_resignal_syntax()
begin
DECLARE foo CONDITION FOR SQLSTATE '12345';
RESIGNAL foo;
end $$
drop procedure test_resignal_syntax $$
create procedure test_resignal_syntax()
begin
RESIGNAL SQLSTATE '23000';
end $$
drop procedure test_resignal_syntax $$
create procedure test_resignal_syntax()
begin
RESIGNAL SQLSTATE VALUE '23000';
end $$
drop procedure test_resignal_syntax $$
create procedure test_resignal_syntax()
begin
RESIGNAL SET CLASS_ORIGIN = 'foo';
end $$
drop procedure test_resignal_syntax $$
create procedure test_resignal_syntax()
begin
DECLARE foo CONDITION FOR SQLSTATE '12345';
RESIGNAL foo SET CLASS_ORIGIN = 'foo';
end $$
drop procedure test_resignal_syntax $$
create procedure test_resignal_syntax()
begin
RESIGNAL SET SUBCLASS_ORIGIN = 'foo';
end $$
drop procedure test_resignal_syntax $$
create procedure test_resignal_syntax()
begin
DECLARE foo CONDITION FOR SQLSTATE '12345';
RESIGNAL foo SET SUBCLASS_ORIGIN = 'foo';
end $$
drop procedure test_resignal_syntax $$
create procedure test_resignal_syntax()
begin
RESIGNAL SET CONSTRAINT_CATALOG = 'foo';
end $$
drop procedure test_resignal_syntax $$
create procedure test_resignal_syntax()
begin
DECLARE foo CONDITION FOR SQLSTATE '12345';
RESIGNAL foo SET CONSTRAINT_CATALOG = 'foo';
end $$
drop procedure test_resignal_syntax $$
create procedure test_resignal_syntax()
begin
RESIGNAL SET CONSTRAINT_SCHEMA = 'foo';
end $$
drop procedure test_resignal_syntax $$
create procedure test_resignal_syntax()
begin
DECLARE foo CONDITION FOR SQLSTATE '12345';
RESIGNAL foo SET CONSTRAINT_SCHEMA = 'foo';
end $$
drop procedure test_resignal_syntax $$
create procedure test_resignal_syntax()
begin
RESIGNAL SET CONSTRAINT_NAME = 'foo';
end $$
drop procedure test_resignal_syntax $$
create procedure test_resignal_syntax()
begin
DECLARE foo CONDITION FOR SQLSTATE '12345';
RESIGNAL foo SET CONSTRAINT_NAME = 'foo';
end $$
drop procedure test_resignal_syntax $$
create procedure test_resignal_syntax()
begin
RESIGNAL SET CATALOG_NAME = 'foo';
end $$
drop procedure test_resignal_syntax $$
create procedure test_resignal_syntax()
begin
DECLARE foo CONDITION FOR SQLSTATE '12345';
RESIGNAL foo SET CATALOG_NAME = 'foo';
end $$
drop procedure test_resignal_syntax $$
create procedure test_resignal_syntax()
begin
RESIGNAL SET SCHEMA_NAME = 'foo';
end $$
drop procedure test_resignal_syntax $$
create procedure test_resignal_syntax()
begin
DECLARE foo CONDITION FOR SQLSTATE '12345';
RESIGNAL foo SET SCHEMA_NAME = 'foo';
end $$
drop procedure test_resignal_syntax $$
create procedure test_resignal_syntax()
begin
RESIGNAL SET TABLE_NAME = 'foo';
end $$
drop procedure test_resignal_syntax $$
create procedure test_resignal_syntax()
begin
DECLARE foo CONDITION FOR SQLSTATE '12345';
RESIGNAL foo SET TABLE_NAME = 'foo';
end $$
drop procedure test_resignal_syntax $$
create procedure test_resignal_syntax()
begin
RESIGNAL SET COLUMN_NAME = 'foo';
end $$
drop procedure test_resignal_syntax $$
create procedure test_resignal_syntax()
begin
DECLARE foo CONDITION FOR SQLSTATE '12345';
RESIGNAL foo SET COLUMN_NAME = 'foo';
end $$
drop procedure test_resignal_syntax $$
create procedure test_resignal_syntax()
begin
RESIGNAL SET CURSOR_NAME = 'foo';
end $$
drop procedure test_resignal_syntax $$
create procedure test_resignal_syntax()
begin
DECLARE foo CONDITION FOR SQLSTATE '12345';
RESIGNAL foo SET CURSOR_NAME = 'foo';
end $$
drop procedure test_resignal_syntax $$
create procedure test_resignal_syntax()
begin
RESIGNAL SET MESSAGE_TEXT = 'foo';
end $$
drop procedure test_resignal_syntax $$
create procedure test_resignal_syntax()
begin
DECLARE foo CONDITION FOR SQLSTATE '12345';
RESIGNAL foo SET MESSAGE_TEXT = 'foo';
end $$
drop procedure test_resignal_syntax $$
create procedure test_resignal_syntax()
begin
RESIGNAL SET MYSQL_ERRNO = 'foo';
end $$
drop procedure test_resignal_syntax $$
create procedure test_resignal_syntax()
begin
DECLARE foo CONDITION FOR SQLSTATE '12345';
RESIGNAL foo SET MYSQL_ERRNO = 'foo';
end $$
drop procedure test_resignal_syntax $$
create procedure test_invalid()
begin
DECLARE foo CONDITION FOR SQLSTATE '12345';
RESIGNAL foo SET CLASS_ORIGIN = 'foo', CLASS_ORIGIN = 'bar';
end $$
ERROR 42000: Duplicate condition information item 'CLASS_ORIGIN'
create procedure test_invalid()
begin
DECLARE foo CONDITION FOR SQLSTATE '12345';
RESIGNAL foo SET MESSAGE_TEXT = 'foo', MESSAGE_TEXT = 'bar';
end $$
ERROR 42000: Duplicate condition information item 'MESSAGE_TEXT'
create procedure test_invalid()
begin
DECLARE foo CONDITION FOR SQLSTATE '12345';
RESIGNAL foo SET MYSQL_ERRNO = 'foo', MYSQL_ERRNO = 'bar';
end $$
ERROR 42000: Duplicate condition information item 'MYSQL_ERRNO'
create procedure test_resignal_syntax()
begin
DECLARE foo CONDITION FOR SQLSTATE '12345';
RESIGNAL foo SET
CLASS_ORIGIN = 'foo',
SUBCLASS_ORIGIN = 'foo',
CONSTRAINT_CATALOG = 'foo',
CONSTRAINT_SCHEMA = 'foo',
CONSTRAINT_NAME = 'foo',
CATALOG_NAME = 'foo',
SCHEMA_NAME = 'foo',
TABLE_NAME = 'foo',
COLUMN_NAME = 'foo',
CURSOR_NAME = 'foo',
MESSAGE_TEXT = 'foo';
end $$
drop procedure test_resignal_syntax $$
create procedure test_invalid()
begin
RESIGNAL SQLSTATE '00000';
end $$
ERROR 42000: Bad SQLSTATE: '00000'
create procedure test_invalid()
begin
RESIGNAL SQLSTATE '00001';
end $$
ERROR 42000: Bad SQLSTATE: '00001'
#
# PART 2: non preparable statements
#
prepare stmt from 'SIGNAL SQLSTATE \'23000\'';
ERROR HY000: This command is not supported in the prepared statement protocol yet
prepare stmt from 'RESIGNAL SQLSTATE \'23000\'';
ERROR HY000: This command is not supported in the prepared statement protocol yet
#
# PART 3: runtime execution
#
drop procedure if exists test_signal;
drop procedure if exists test_resignal;
drop table if exists t_warn;
drop table if exists t_cursor;
create table t_warn(a integer(2));
Warnings:
Warning	1681	Integer display width is deprecated and will be removed in a future release.
create table t_cursor(a integer);
#
# SIGNAL can also appear in a query
#
SIGNAL foo;
ERROR 42000: Undefined CONDITION: foo
SIGNAL SQLSTATE '01000';
Warnings:
Warning	1642	Unhandled user-defined warning condition
SIGNAL SQLSTATE '02000';
ERROR 02000: Unhandled user-defined not found condition
SIGNAL SQLSTATE '23000';
ERROR 23000: Unhandled user-defined exception condition
SIGNAL SQLSTATE VALUE '23000';
ERROR 23000: Unhandled user-defined exception condition
SIGNAL SQLSTATE 'HY000' SET MYSQL_ERRNO = 65536;
ERROR 42000: Variable 'MYSQL_ERRNO' can't be set to the value of '65536'
SIGNAL SQLSTATE 'HY000' SET MYSQL_ERRNO = 99999;
ERROR 42000: Variable 'MYSQL_ERRNO' can't be set to the value of '99999'
SIGNAL SQLSTATE 'HY000' SET MYSQL_ERRNO = 4294967295;
ERROR 42000: Variable 'MYSQL_ERRNO' can't be set to the value of '4294967295'
SIGNAL SQLSTATE 'HY000' SET MYSQL_ERRNO = 0;
ERROR 42000: Variable 'MYSQL_ERRNO' can't be set to the value of '0'
SIGNAL SQLSTATE 'HY000' SET MYSQL_ERRNO = -1;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near '-1' at line 1
SIGNAL SQLSTATE 'HY000' SET MYSQL_ERRNO = 65535;
ERROR HY000: Unhandled user-defined exception condition
#
# RESIGNAL can also appear in a query
#
RESIGNAL;
ERROR 0K000: RESIGNAL when handler not active
RESIGNAL foo;
ERROR 42000: Undefined CONDITION: foo
RESIGNAL SQLSTATE '12345';
ERROR 0K000: RESIGNAL when handler not active
RESIGNAL SQLSTATE VALUE '12345';
ERROR 0K000: RESIGNAL when handler not active
#
# Different kind of SIGNAL conditions
#
create procedure test_signal()
begin
# max range
DECLARE foo CONDITION FOR SQLSTATE 'AABBB';
SIGNAL foo SET MYSQL_ERRNO = 65535;
end $$
call test_signal() $$
ERROR AABBB: Unhandled user-defined exception condition
drop procedure test_signal $$
create procedure test_signal()
begin
# max range
DECLARE foo CONDITION FOR SQLSTATE 'AABBB';
SIGNAL foo SET MYSQL_ERRNO = 65536;
end $$
call test_signal() $$
ERROR 42000: Variable 'MYSQL_ERRNO' can't be set to the value of '65536'
drop procedure test_signal $$
create procedure test_signal()
begin
# Error
DECLARE foo CONDITION FOR SQLSTATE '99999';
SIGNAL foo SET MYSQL_ERRNO = 9999;
end $$
call test_signal() $$
ERROR 99999: Unhandled user-defined exception condition
drop procedure test_signal $$
create procedure test_signal()
begin
# warning
DECLARE too_few_records CONDITION FOR SQLSTATE '01000';
SIGNAL too_few_records SET MYSQL_ERRNO = 1261;
end $$
call test_signal() $$
Warnings:
Warning	1261	Unhandled user-defined warning condition
drop procedure test_signal $$
create procedure test_signal()
begin
# Not found
DECLARE sp_fetch_no_data CONDITION FOR SQLSTATE '02000';
SIGNAL sp_fetch_no_data SET MYSQL_ERRNO = 1329;
end $$
call test_signal() $$
ERROR 02000: Unhandled user-defined not found condition
drop procedure test_signal $$
create procedure test_signal()
begin
# Error
DECLARE sp_cursor_already_open CONDITION FOR SQLSTATE '24000';
SIGNAL sp_cursor_already_open SET MYSQL_ERRNO = 1325;
end $$
call test_signal() $$
ERROR 24000: Unhandled user-defined exception condition
drop procedure test_signal $$
create procedure test_signal()
begin
# Severe error
DECLARE lock_deadlock CONDITION FOR SQLSTATE '40001';
SIGNAL lock_deadlock SET MYSQL_ERRNO = 1213;
end $$
call test_signal() $$
ERROR 40001: Unhandled user-defined exception condition
drop procedure test_signal $$
create procedure test_signal()
begin
# Unknown -> error
DECLARE foo CONDITION FOR SQLSTATE "99999";
SIGNAL foo;
end $$
call test_signal() $$
ERROR 99999: Unhandled user-defined exception condition
drop procedure test_signal $$
create procedure test_signal()
begin
# warning, no subclass
DECLARE warn CONDITION FOR SQLSTATE "01000";
SIGNAL warn;
end $$
call test_signal() $$
Warnings:
Warning	1642	Unhandled user-defined warning condition
drop procedure test_signal $$
create procedure test_signal()
begin
# warning, with subclass
DECLARE warn CONDITION FOR SQLSTATE "01123";
SIGNAL warn;
end $$
call test_signal() $$
Warnings:
Warning	1642	Unhandled user-defined warning condition
drop procedure test_signal $$
create procedure test_signal()
begin
# Not found, no subclass
DECLARE not_found CONDITION FOR SQLSTATE "02000";
SIGNAL not_found;
end $$
call test_signal() $$
ERROR 02000: Unhandled user-defined not found condition
drop procedure test_signal $$
create procedure test_signal()
begin
# Not found, with subclass
DECLARE not_found CONDITION FOR SQLSTATE "02XXX";
SIGNAL not_found;
end $$
call test_signal() $$
ERROR 02XXX: Unhandled user-defined not found condition
drop procedure test_signal $$
create procedure test_signal()
begin
# Error, no subclass
DECLARE error CONDITION FOR SQLSTATE "12000";
SIGNAL error;
end $$
call test_signal() $$
ERROR 12000: Unhandled user-defined exception condition
drop procedure test_signal $$
create procedure test_signal()
begin
# Error, with subclass
DECLARE error CONDITION FOR SQLSTATE "12ABC";
SIGNAL error;
end $$
call test_signal() $$
ERROR 12ABC: Unhandled user-defined exception condition
drop procedure test_signal $$
create procedure test_signal()
begin
# Severe error, no subclass
DECLARE error CONDITION FOR SQLSTATE "40000";
SIGNAL error;
end $$
call test_signal() $$
ERROR 40000: Unhandled user-defined exception condition
drop procedure test_signal $$
create procedure test_signal()
begin
# Severe error, with subclass
DECLARE error CONDITION FOR SQLSTATE "40001";
SIGNAL error;
end $$
call test_signal() $$
ERROR 40001: Unhandled user-defined exception condition
drop procedure test_signal $$
#
# Test the scope of condition
#
create procedure test_signal()
begin
DECLARE foo CONDITION FOR SQLSTATE '99999';
begin
DECLARE foo CONDITION FOR 8888;
end;
SIGNAL foo SET MYSQL_ERRNO=9999; /* outer */
end $$
call test_signal() $$
ERROR 99999: Unhandled user-defined exception condition
drop procedure test_signal $$
create procedure test_signal()
begin
DECLARE foo CONDITION FOR 9999;
begin
DECLARE foo CONDITION FOR SQLSTATE '88888';
SIGNAL foo SET MYSQL_ERRNO=8888; /* inner */
end;
end $$
call test_signal() $$
ERROR 88888: Unhandled user-defined exception condition
drop procedure test_signal $$
#
# Test SET MYSQL_ERRNO
#
create procedure test_signal()
begin
DECLARE foo CONDITION FOR SQLSTATE '99999';
SIGNAL foo SET MYSQL_ERRNO = 1111;
end $$
call test_signal() $$
ERROR 99999: Unhandled user-defined exception condition
drop procedure test_signal $$
create procedure test_signal()
begin
DECLARE warn CONDITION FOR SQLSTATE "01000";
SIGNAL warn SET MYSQL_ERRNO = 1111;
end $$
call test_signal() $$
Warnings:
Warning	1111	Unhandled user-defined warning condition
drop procedure test_signal $$
create procedure test_signal()
begin
DECLARE not_found CONDITION FOR SQLSTATE "02000";
SIGNAL not_found SET MYSQL_ERRNO = 1111;
end $$
call test_signal() $$
ERROR 02000: Unhandled user-defined not found condition
drop procedure test_signal $$
create procedure test_signal()
begin
DECLARE error CONDITION FOR SQLSTATE "55000";
SIGNAL error SET MYSQL_ERRNO = 1111;
end $$
call test_signal() $$
ERROR 55000: Unhandled user-defined exception condition
drop procedure test_signal $$
#
# Test SET MESSAGE_TEXT
#
SIGNAL SQLSTATE '77777' SET MESSAGE_TEXT='' $$
ERROR 77777: 
create procedure test_signal()
begin
DECLARE foo CONDITION FOR SQLSTATE '77777';
SIGNAL foo SET
MESSAGE_TEXT = "",
MYSQL_ERRNO=45678;
end $$
call test_signal() $$
ERROR 77777: 
drop procedure test_signal $$
create procedure test_signal()
begin
DECLARE foo CONDITION FOR SQLSTATE '99999';
SIGNAL foo SET
MESSAGE_TEXT = "Something bad happened",
MYSQL_ERRNO=9999;
end $$
call test_signal() $$
ERROR 99999: Something bad happened
drop procedure test_signal $$
create procedure test_signal()
begin
DECLARE warn CONDITION FOR SQLSTATE "01000";
SIGNAL warn SET MESSAGE_TEXT = "Something bad happened";
end $$
call test_signal() $$
Warnings:
Warning	1642	Something bad happened
drop procedure test_signal $$
create procedure test_signal()
begin
DECLARE not_found CONDITION FOR SQLSTATE "02000";
SIGNAL not_found SET MESSAGE_TEXT = "Something bad happened";
end $$
call test_signal() $$
ERROR 02000: Something bad happened
drop procedure test_signal $$
create procedure test_signal()
begin
DECLARE error CONDITION FOR SQLSTATE "55000";
SIGNAL error SET MESSAGE_TEXT = "Something bad happened";
end $$
call test_signal() $$
ERROR 55000: Something bad happened
drop procedure test_signal $$
create procedure test_signal()
begin
DECLARE something CONDITION FOR SQLSTATE "01000";
SIGNAL something SET MESSAGE_TEXT = _utf8 "This is a UTF8 text";
end $$
Warnings:
Warning	3719	'utf8' is currently an alias for the character set UTF8MB3, but will be an alias for UTF8MB4 in a future release. Please consider using UTF8MB4 in order to be unambiguous.
call test_signal() $$
Warnings:
Warning	3719	'utf8' is currently an alias for the character set UTF8MB3, but will be an alias for UTF8MB4 in a future release. Please consider using UTF8MB4 in order to be unambiguous.
Warning	1642	This is a UTF8 text
drop procedure test_signal $$
create procedure test_signal()
begin
DECLARE something CONDITION FOR SQLSTATE "01000";
SIGNAL something SET MESSAGE_TEXT = "";
end $$
call test_signal() $$
Warnings:
Warning	1642	
drop procedure test_signal $$
create procedure test_signal()
begin
DECLARE warn CONDITION FOR SQLSTATE "01111";
SIGNAL warn SET MESSAGE_TEXT = "á a";
end $$
call test_signal() $$
Warnings:
Warning	1642	á a
show warnings $$
Level	Code	Message
Warning	1642	á a
drop procedure test_signal $$
#
# Test SET complex expressions
#
create procedure test_signal()
begin
DECLARE error CONDITION FOR SQLSTATE '99999';
SIGNAL error SET
MYSQL_ERRNO = NULL;
end $$
call test_signal() $$
ERROR 42000: Variable 'MYSQL_ERRNO' can't be set to the value of 'NULL'
drop procedure test_signal $$
create procedure test_signal()
begin
DECLARE error CONDITION FOR SQLSTATE '99999';
SIGNAL error SET
CLASS_ORIGIN = NULL;
end $$
call test_signal() $$
ERROR 42000: Variable 'CLASS_ORIGIN' can't be set to the value of 'NULL'
drop procedure test_signal $$
create procedure test_signal()
begin
DECLARE error CONDITION FOR SQLSTATE '99999';
SIGNAL error SET
SUBCLASS_ORIGIN = NULL;
end $$
call test_signal() $$
ERROR 42000: Variable 'SUBCLASS_ORIGIN' can't be set to the value of 'NULL'
drop procedure test_signal $$
create procedure test_signal()
begin
DECLARE error CONDITION FOR SQLSTATE '99999';
SIGNAL error SET
CONSTRAINT_CATALOG = NULL;
end $$
call test_signal() $$
ERROR 42000: Variable 'CONSTRAINT_CATALOG' can't be set to the value of 'NULL'
drop procedure test_signal $$
create procedure test_signal()
begin
DECLARE error CONDITION FOR SQLSTATE '99999';
SIGNAL error SET
CONSTRAINT_SCHEMA = NULL;
end $$
call test_signal() $$
ERROR 42000: Variable 'CONSTRAINT_SCHEMA' can't be set to the value of 'NULL'
drop procedure test_signal $$
create procedure test_signal()
begin
DECLARE error CONDITION FOR SQLSTATE '99999';
SIGNAL error SET
CONSTRAINT_NAME = NULL;
end $$
call test_signal() $$
ERROR 42000: Variable 'CONSTRAINT_NAME' can't be set to the value of 'NULL'
drop procedure test_signal $$
create procedure test_signal()
begin
DECLARE error CONDITION FOR SQLSTATE '99999';
SIGNAL error SET
CATALOG_NAME = NULL;
end $$
call test_signal() $$
ERROR 42000: Variable 'CATALOG_NAME' can't be set to the value of 'NULL'
drop procedure test_signal $$
create procedure test_signal()
begin
DECLARE error CONDITION FOR SQLSTATE '99999';
SIGNAL error SET
SCHEMA_NAME = NULL;
end $$
call test_signal() $$
ERROR 42000: Variable 'SCHEMA_NAME' can't be set to the value of 'NULL'
drop procedure test_signal $$
create procedure test_signal()
begin
DECLARE error CONDITION FOR SQLSTATE '99999';
SIGNAL error SET
TABLE_NAME = NULL;
end $$
call test_signal() $$
ERROR 42000: Variable 'TABLE_NAME' can't be set to the value of 'NULL'
drop procedure test_signal $$
create procedure test_signal()
begin
DECLARE error CONDITION FOR SQLSTATE '99999';
SIGNAL error SET
COLUMN_NAME = NULL;
end $$
call test_signal() $$
ERROR 42000: Variable 'COLUMN_NAME' can't be set to the value of 'NULL'
drop procedure test_signal $$
create procedure test_signal()
begin
DECLARE error CONDITION FOR SQLSTATE '99999';
SIGNAL error SET
CURSOR_NAME = NULL;
end $$
call test_signal() $$
ERROR 42000: Variable 'CURSOR_NAME' can't be set to the value of 'NULL'
drop procedure test_signal $$
create procedure test_signal()
begin
DECLARE error CONDITION FOR SQLSTATE '99999';
SIGNAL error SET
MESSAGE_TEXT = NULL;
end $$
call test_signal() $$
ERROR 42000: Variable 'MESSAGE_TEXT' can't be set to the value of 'NULL'
drop procedure test_signal $$
create procedure test_signal()
begin
DECLARE something CONDITION FOR SQLSTATE '99999';
DECLARE message_text VARCHAR(64) DEFAULT "Local string variable";
DECLARE sqlcode INTEGER DEFAULT 1234;
SIGNAL something SET
MESSAGE_TEXT = message_text,
MYSQL_ERRNO = sqlcode;
end $$
call test_signal() $$
ERROR 99999: Local string variable
drop procedure test_signal $$
create procedure test_signal(message_text VARCHAR(64), sqlcode INTEGER)
begin
DECLARE something CONDITION FOR SQLSTATE "12345";
SIGNAL something SET
MESSAGE_TEXT = message_text,
MYSQL_ERRNO = sqlcode;
end $$
call test_signal("Parameter string", NULL) $$
ERROR 42000: Variable 'MYSQL_ERRNO' can't be set to the value of 'NULL'
call test_signal(NULL, 1234) $$
ERROR 42000: Variable 'MESSAGE_TEXT' can't be set to the value of 'NULL'
call test_signal("Parameter string", 45678) $$
ERROR 12345: Parameter string
drop procedure test_signal $$
create procedure test_signal()
begin
DECLARE something CONDITION FOR SQLSTATE "AABBB";
SIGNAL something SET
MESSAGE_TEXT = @message_text,
MYSQL_ERRNO = @sqlcode;
end $$
call test_signal() $$
ERROR 42000: Variable 'MESSAGE_TEXT' can't be set to the value of 'NULL'
set @sqlcode= 12 $$
call test_signal() $$
ERROR 42000: Variable 'MESSAGE_TEXT' can't be set to the value of 'NULL'
set @message_text= "User variable" $$
call test_signal() $$
ERROR AABBB: User variable
drop procedure test_signal $$
create procedure test_invalid()
begin
DECLARE something CONDITION FOR SQLSTATE "AABBB";
SIGNAL something SET
MESSAGE_TEXT = @message_text := 'illegal',
MYSQL_ERRNO = @sqlcode := 1234;
end $$
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near ':= 'illegal',
MYSQL_ERRNO = @sqlcode := 1234;
end' at line 5
create procedure test_signal()
begin
DECLARE aaa VARCHAR(64);
DECLARE bbb VARCHAR(64);
DECLARE ccc VARCHAR(64);
DECLARE ddd VARCHAR(64);
DECLARE eee VARCHAR(64);
DECLARE fff VARCHAR(64);
DECLARE ggg VARCHAR(64);
DECLARE hhh VARCHAR(64);
DECLARE iii VARCHAR(64);
DECLARE jjj VARCHAR(64);
DECLARE kkk VARCHAR(64);
DECLARE warn CONDITION FOR SQLSTATE "01234";
set aaa= repeat("A", 64);
set bbb= repeat("B", 64);
set ccc= repeat("C", 64);
set ddd= repeat("D", 64);
set eee= repeat("E", 64);
set fff= repeat("F", 64);
set ggg= repeat("G", 64);
set hhh= repeat("H", 64);
set iii= repeat("I", 64);
set jjj= repeat("J", 64);
set kkk= repeat("K", 64);
SIGNAL warn SET
CLASS_ORIGIN = aaa,
SUBCLASS_ORIGIN = bbb,
CONSTRAINT_CATALOG = ccc,
CONSTRAINT_SCHEMA = ddd,
CONSTRAINT_NAME = eee,
CATALOG_NAME = fff,
SCHEMA_NAME = ggg,
TABLE_NAME = hhh,
COLUMN_NAME = iii,
CURSOR_NAME = jjj,
MESSAGE_TEXT = kkk,
MYSQL_ERRNO = 65535;
end $$
call test_signal() $$
Warnings:
Warning	65535	KKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKK
drop procedure test_signal $$
create procedure test_signal()
begin
DECLARE warn CONDITION FOR SQLSTATE "01234";
SIGNAL warn SET
MYSQL_ERRNO = 999999999999999999999999999999999999999999999999999;
end $$
call test_signal() $$
ERROR 42000: Variable 'MYSQL_ERRNO' can't be set to the value of '999999999999999999999999999999999999999999999999999'
drop procedure test_signal $$
SET sql_mode = 'NO_ENGINE_SUBSTITUTION';
create procedure test_signal()
begin
DECLARE aaax VARCHAR(65);
DECLARE bbbx VARCHAR(65);
DECLARE cccx VARCHAR(65);
DECLARE dddx VARCHAR(65);
DECLARE eeex VARCHAR(65);
DECLARE fffx VARCHAR(65);
DECLARE gggx VARCHAR(65);
DECLARE hhhx VARCHAR(65);
DECLARE iiix VARCHAR(65);
DECLARE jjjx VARCHAR(65);
DECLARE kkkx VARCHAR(65);
DECLARE lllx VARCHAR(129);
DECLARE warn CONDITION FOR SQLSTATE "01234";
set aaax= concat(repeat("A", 64), "X");
set bbbx= concat(repeat("B", 64), "X");
set cccx= concat(repeat("C", 64), "X");
set dddx= concat(repeat("D", 64), "X");
set eeex= concat(repeat("E", 64), "X");
set fffx= concat(repeat("F", 64), "X");
set gggx= concat(repeat("G", 64), "X");
set hhhx= concat(repeat("H", 64), "X");
set iiix= concat(repeat("I", 64), "X");
set jjjx= concat(repeat("J", 64), "X");
set kkkx= concat(repeat("K", 64), "X");
set lllx= concat(repeat("1", 100),
repeat("2", 20),
repeat("8", 8),
"X");
SIGNAL warn SET
CLASS_ORIGIN = aaax,
SUBCLASS_ORIGIN = bbbx,
CONSTRAINT_CATALOG = cccx,
CONSTRAINT_SCHEMA = dddx,
CONSTRAINT_NAME = eeex,
CATALOG_NAME = fffx,
SCHEMA_NAME = gggx,
TABLE_NAME = hhhx,
COLUMN_NAME = iiix,
CURSOR_NAME = jjjx,
MESSAGE_TEXT = lllx,
MYSQL_ERRNO = 10000;
end $$
call test_signal() $$
Warnings:
Warning	1647	Data truncated for condition item 'CLASS_ORIGIN'
Warning	1647	Data truncated for condition item 'SUBCLASS_ORIGIN'
Warning	1647	Data truncated for condition item 'CONSTRAINT_CATALOG'
Warning	1647	Data truncated for condition item 'CONSTRAINT_SCHEMA'
Warning	1647	Data truncated for condition item 'CONSTRAINT_NAME'
Warning	1647	Data truncated for condition item 'CATALOG_NAME'
Warning	1647	Data truncated for condition item 'SCHEMA_NAME'
Warning	1647	Data truncated for condition item 'TABLE_NAME'
Warning	1647	Data truncated for condition item 'COLUMN_NAME'
Warning	1647	Data truncated for condition item 'CURSOR_NAME'
Warning	1647	Data truncated for condition item 'MESSAGE_TEXT'
Warning	10000	11111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111112222222222222222222288888888
drop procedure test_signal $$
SET sql_mode = default;
# Test that HANDLER can catch conditions raised by SIGNAL
create procedure test_signal()
begin
DECLARE warn CONDITION FOR SQLSTATE "01234";
DECLARE CONTINUE HANDLER for SQLSTATE "01234"
  begin
select "Caught by SQLSTATE";
end;
SIGNAL warn SET
MESSAGE_TEXT = "Raising a warning",
MYSQL_ERRNO = 1012;
end $$
call test_signal() $$
Caught by SQLSTATE
Caught by SQLSTATE
drop procedure test_signal $$
create procedure test_signal()
begin
DECLARE warn CONDITION FOR SQLSTATE "01234";
DECLARE CONTINUE HANDLER for 1012
begin
select "Caught by number";
end;
SIGNAL warn SET
MESSAGE_TEXT = "Raising a warning",
MYSQL_ERRNO = 1012;
end $$
call test_signal() $$
Caught by number
Caught by number
drop procedure test_signal $$
create procedure test_signal()
begin
DECLARE warn CONDITION FOR SQLSTATE "01234";
DECLARE CONTINUE HANDLER for SQLWARNING
begin
select "Caught by SQLWARNING";
end;
SIGNAL warn SET
MESSAGE_TEXT = "Raising a warning",
MYSQL_ERRNO = 1012;
end $$
call test_signal() $$
Caught by SQLWARNING
Caught by SQLWARNING
drop procedure test_signal $$
create procedure test_signal()
begin
DECLARE not_found CONDITION FOR SQLSTATE "02ABC";
DECLARE CONTINUE HANDLER for SQLSTATE "02ABC"
  begin
select "Caught by SQLSTATE";
end;
SIGNAL not_found SET
MESSAGE_TEXT = "Raising a not found",
MYSQL_ERRNO = 1012;
end $$
call test_signal() $$
Caught by SQLSTATE
Caught by SQLSTATE
drop procedure test_signal $$
create procedure test_signal()
begin
DECLARE not_found CONDITION FOR SQLSTATE "02ABC";
DECLARE CONTINUE HANDLER for 1012
begin
select "Caught by number";
end;
SIGNAL not_found SET
MESSAGE_TEXT = "Raising a not found",
MYSQL_ERRNO = 1012;
end $$
call test_signal() $$
Caught by number
Caught by number
drop procedure test_signal $$
create procedure test_signal()
begin
DECLARE not_found CONDITION FOR SQLSTATE "02ABC";
DECLARE CONTINUE HANDLER for NOT FOUND
begin
select "Caught by NOT FOUND";
end;
SIGNAL not_found SET
MESSAGE_TEXT = "Raising a not found",
MYSQL_ERRNO = 1012;
end $$
call test_signal() $$
Caught by NOT FOUND
Caught by NOT FOUND
drop procedure test_signal $$
create procedure test_signal()
begin
DECLARE error CONDITION FOR SQLSTATE "55555";
DECLARE CONTINUE HANDLER for SQLSTATE "55555"
  begin
select "Caught by SQLSTATE";
end;
SIGNAL error SET
MESSAGE_TEXT = "Raising an error",
MYSQL_ERRNO = 1012;
end $$
call test_signal() $$
Caught by SQLSTATE
Caught by SQLSTATE
drop procedure test_signal $$
create procedure test_signal()
begin
DECLARE error CONDITION FOR SQLSTATE "55555";
DECLARE CONTINUE HANDLER for 1012
begin
select "Caught by number";
end;
SIGNAL error SET
MESSAGE_TEXT = "Raising an error",
MYSQL_ERRNO = 1012;
end $$
call test_signal() $$
Caught by number
Caught by number
drop procedure test_signal $$
create procedure test_signal()
begin
DECLARE error CONDITION FOR SQLSTATE "55555";
DECLARE CONTINUE HANDLER for SQLEXCEPTION
begin
select "Caught by SQLEXCEPTION";
end;
SIGNAL error SET
MESSAGE_TEXT = "Raising an error",
MYSQL_ERRNO = 1012;
end $$
call test_signal() $$
Caught by SQLEXCEPTION
Caught by SQLEXCEPTION
drop procedure test_signal $$
#
# Test where SIGNAL can be used
#

# RETURN statement clears Diagnostics Area, thus
# the warnings raised in a stored function are not
# visible outsidef the stored function. So, we're using
# @@warning_count variable to check that SIGNAL succeeded.

create function test_signal_func() returns integer
begin
DECLARE v INT;
DECLARE warn CONDITION FOR SQLSTATE "01XXX";
SIGNAL warn SET
MESSAGE_TEXT = "This function SIGNAL a warning",
MYSQL_ERRNO = 1012;
SELECT @@warning_count INTO v;
return v;
end $$
select test_signal_func() $$
test_signal_func()
1
drop function test_signal_func $$
create function test_signal_func() returns integer
begin
DECLARE not_found CONDITION FOR SQLSTATE "02XXX";
SIGNAL not_found SET
MESSAGE_TEXT = "This function SIGNAL not found",
MYSQL_ERRNO = 1012;
return 5;
end $$
select test_signal_func() $$
ERROR 02XXX: This function SIGNAL not found
drop function test_signal_func $$
create function test_signal_func() returns integer
begin
DECLARE error CONDITION FOR SQLSTATE "50000";
SIGNAL error SET
MESSAGE_TEXT = "This function SIGNAL an error",
MYSQL_ERRNO = 1012;
return 5;
end $$
select test_signal_func() $$
ERROR 50000: This function SIGNAL an error
drop function test_signal_func $$
drop table if exists t1 $$
create table t1 (a integer) $$
create trigger t1_ai after insert on t1 for each row
begin
DECLARE msg VARCHAR(128);
DECLARE warn CONDITION FOR SQLSTATE "01XXX";
set msg= concat("This trigger SIGNAL a warning, a=", NEW.a);
SIGNAL warn SET
MESSAGE_TEXT = msg,
MYSQL_ERRNO = 1012;
end $$
insert into t1 values (1), (2) $$
drop trigger t1_ai $$
create trigger t1_ai after insert on t1 for each row
begin
DECLARE msg VARCHAR(128);
DECLARE not_found CONDITION FOR SQLSTATE "02XXX";
set msg= concat("This trigger SIGNAL a not found, a=", NEW.a);
SIGNAL not_found SET
MESSAGE_TEXT = msg,
MYSQL_ERRNO = 1012;
end $$
insert into t1 values (3), (4) $$
ERROR 02XXX: This trigger SIGNAL a not found, a=3
drop trigger t1_ai $$
create trigger t1_ai after insert on t1 for each row
begin
DECLARE msg VARCHAR(128);
DECLARE error CONDITION FOR SQLSTATE "03XXX";
set msg= concat("This trigger SIGNAL an error, a=", NEW.a);
SIGNAL error SET
MESSAGE_TEXT = msg,
MYSQL_ERRNO = 1012;
end $$
insert into t1 values (5), (6) $$
ERROR 03XXX: This trigger SIGNAL an error, a=5
drop table t1 $$
create table t1 (errno integer, msg varchar(128)) $$
create trigger t1_ai after insert on t1 for each row
begin
DECLARE warn CONDITION FOR SQLSTATE "01XXX";
SIGNAL warn SET
MESSAGE_TEXT = NEW.msg,
MYSQL_ERRNO = NEW.errno;
end $$
insert into t1 set errno=1012, msg='Warning message 1 in trigger' $$
insert into t1 set errno=1013, msg='Warning message 2 in trigger' $$
drop table t1 $$
drop table if exists t1 $$
drop procedure if exists p1 $$
drop function if exists f1 $$
create table t1 (s1 int) $$
insert into t1 values (1) $$
create procedure p1()
begin
declare a int;
declare c cursor for select f1() from t1;
declare continue handler for sqlstate '03000'
    select "caught 03000";
declare continue handler for 1326
select "caught cursor is not open";
select "Before open";
open c;
select "Before fetch";
fetch c into a;
select "Before close";
close c;
end $$
create function f1() returns int
begin
signal sqlstate '03000';
return 5;
end $$
drop table t1 $$
drop procedure p1 $$
drop function f1 $$
#
# Test the RESIGNAL runtime
#
create procedure test_resignal()
begin
DECLARE warn CONDITION FOR SQLSTATE "01234";
DECLARE CONTINUE HANDLER for 1012
begin
select "handler called";
RESIGNAL;
end;
SIGNAL warn SET
MESSAGE_TEXT = "Raising a warning",
MYSQL_ERRNO = 1012;
end $$
call test_resignal() $$
handler called
handler called
Warnings:
Warning	1012	Raising a warning
drop procedure test_resignal $$
create procedure test_resignal()
begin
DECLARE not_found CONDITION FOR SQLSTATE "02222";
DECLARE CONTINUE HANDLER for 1012
begin
select "before RESIGNAL";
RESIGNAL;
select "after RESIGNAL";
end;
SIGNAL not_found SET
MESSAGE_TEXT = "Raising a not found",
MYSQL_ERRNO = 1012;
end $$
call test_resignal() $$
before RESIGNAL
before RESIGNAL
ERROR 02222: Raising a not found
drop procedure test_resignal $$
create procedure test_resignal()
begin
DECLARE error CONDITION FOR SQLSTATE "55555";
DECLARE CONTINUE HANDLER for 1012
begin
select "before RESIGNAL";
RESIGNAL;
select "after RESIGNAL";
end;
SIGNAL error SET
MESSAGE_TEXT = "Raising an error",
MYSQL_ERRNO = 1012;
end $$
call test_resignal() $$
before RESIGNAL
before RESIGNAL
ERROR 55555: Raising an error
drop procedure test_resignal $$
SET sql_mode = 'NO_ENGINE_SUBSTITUTION';
create procedure test_resignal()
begin
DECLARE CONTINUE HANDLER for sqlwarning
begin
select "handler called";
RESIGNAL;
end;
insert into t_warn set a= 9999999999999999;
end $$
call test_resignal() $$
handler called
handler called
Warnings:
Warning	1264	Out of range value for column 'a' at row 1
drop procedure test_resignal $$
SET sql_mode = default;
create procedure test_resignal()
begin
DECLARE x integer;
DECLARE c cursor for select * from t_cursor;
DECLARE CONTINUE HANDLER for not found
begin
select "before RESIGNAL";
RESIGNAL;
select "after RESIGNAL";
end;
open c;
fetch c into x;
close c;
end $$
call test_resignal() $$
before RESIGNAL
before RESIGNAL
ERROR 02000: No data - zero rows fetched, selected, or processed
drop procedure test_resignal $$
create procedure test_resignal()
begin
DECLARE CONTINUE HANDLER for sqlexception
begin
select "before RESIGNAL";
RESIGNAL;
select "after RESIGNAL";
end;
drop table no_such_table;
end $$
call test_resignal() $$
before RESIGNAL
before RESIGNAL
ERROR 42S02: Unknown table 'test.no_such_table'
drop procedure test_resignal $$
create procedure test_resignal()
begin
DECLARE warn CONDITION FOR SQLSTATE "01234";
DECLARE CONTINUE HANDLER for 1012
begin
select "handler called";
RESIGNAL SET
MESSAGE_TEXT = "RESIGNAL of a warning",
MYSQL_ERRNO = 55555 ;
end;
SIGNAL warn SET
MESSAGE_TEXT = "Raising a warning",
MYSQL_ERRNO = 1012;
end $$
call test_resignal() $$
handler called
handler called
Warnings:
Warning	55555	RESIGNAL of a warning
drop procedure test_resignal $$
create procedure test_resignal()
begin
DECLARE not_found CONDITION FOR SQLSTATE "02111";
DECLARE CONTINUE HANDLER for 1012
begin
select "before RESIGNAL";
RESIGNAL SET
MESSAGE_TEXT = "RESIGNAL of a not found",
MYSQL_ERRNO = 55555 ;
select "after RESIGNAL";
end;
SIGNAL not_found SET
MESSAGE_TEXT = "Raising a not found",
MYSQL_ERRNO = 1012;
end $$
call test_resignal() $$
before RESIGNAL
before RESIGNAL
ERROR 02111: RESIGNAL of a not found
drop procedure test_resignal $$
create procedure test_resignal()
begin
DECLARE error CONDITION FOR SQLSTATE "33333";
DECLARE CONTINUE HANDLER for 1012
begin
select "before RESIGNAL";
RESIGNAL SET
MESSAGE_TEXT = "RESIGNAL of an error",
MYSQL_ERRNO = 55555 ;
select "after RESIGNAL";
end;
SIGNAL error SET
MESSAGE_TEXT = "Raising an error",
MYSQL_ERRNO = 1012;
end $$
call test_resignal() $$
before RESIGNAL
before RESIGNAL
ERROR 33333: RESIGNAL of an error
drop procedure test_resignal $$
SET sql_mode = 'NO_ENGINE_SUBSTITUTION';
create procedure test_resignal()
begin
DECLARE CONTINUE HANDLER for sqlwarning
begin
select "handler called";
RESIGNAL SET
MESSAGE_TEXT = "RESIGNAL of a warning",
MYSQL_ERRNO = 55555 ;
end;
insert into t_warn set a= 9999999999999999;
end $$
call test_resignal() $$
handler called
handler called
Warnings:
Warning	55555	RESIGNAL of a warning
drop procedure test_resignal $$
SET sql_mode = default;
create procedure test_resignal()
begin
DECLARE x integer;
DECLARE c cursor for select * from t_cursor;
DECLARE CONTINUE HANDLER for not found
begin
select "before RESIGNAL";
RESIGNAL SET
MESSAGE_TEXT = "RESIGNAL of not found",
MYSQL_ERRNO = 55555 ;
select "after RESIGNAL";
end;
open c;
fetch c into x;
close c;
end $$
call test_resignal() $$
before RESIGNAL
before RESIGNAL
ERROR 02000: RESIGNAL of not found
drop procedure test_resignal $$
create procedure test_resignal()
begin
DECLARE CONTINUE HANDLER for sqlexception
begin
select "before RESIGNAL";
RESIGNAL SET
MESSAGE_TEXT = "RESIGNAL of an error",
MYSQL_ERRNO = 55555 ;
select "after RESIGNAL";
end;
drop table no_such_table;
end $$
call test_resignal() $$
before RESIGNAL
before RESIGNAL
ERROR 42S02: RESIGNAL of an error
drop procedure test_resignal $$
create procedure test_resignal()
begin
DECLARE warn CONDITION FOR SQLSTATE "01111";
DECLARE CONTINUE HANDLER for 1012
begin
select "handler called";
RESIGNAL SQLSTATE "01222" SET
MESSAGE_TEXT = "RESIGNAL to warning",
MYSQL_ERRNO = 55555 ;
end;
SIGNAL warn SET
MESSAGE_TEXT = "Raising a warning",
MYSQL_ERRNO = 1012;
end $$
call test_resignal() $$
handler called
handler called
Warnings:
Warning	1012	Raising a warning
Warning	55555	RESIGNAL to warning
drop procedure test_resignal $$
create procedure test_resignal()
begin
DECLARE warn CONDITION FOR SQLSTATE "01111";
DECLARE CONTINUE HANDLER for 1012
begin
select "before RESIGNAL";
RESIGNAL SQLSTATE "02222" SET
MESSAGE_TEXT = "RESIGNAL to not found",
MYSQL_ERRNO = 55555 ;
select "after RESIGNAL";
end;
SIGNAL warn SET
MESSAGE_TEXT = "Raising a warning",
MYSQL_ERRNO = 1012;
end $$
call test_resignal() $$
before RESIGNAL
before RESIGNAL
ERROR 02222: RESIGNAL to not found
show warnings $$
Level	Code	Message
Warning	1012	Raising a warning
Error	55555	RESIGNAL to not found
drop procedure test_resignal $$
create procedure test_resignal()
begin
DECLARE warn CONDITION FOR SQLSTATE "01111";
DECLARE CONTINUE HANDLER for 1012
begin
select "before RESIGNAL";
RESIGNAL SQLSTATE "33333" SET
MESSAGE_TEXT = "RESIGNAL to error",
MYSQL_ERRNO = 55555 ;
select "after RESIGNAL";
end;
SIGNAL warn SET
MESSAGE_TEXT = "Raising a warning",
MYSQL_ERRNO = 1012;
end $$
call test_resignal() $$
before RESIGNAL
before RESIGNAL
ERROR 33333: RESIGNAL to error
show warnings $$
Level	Code	Message
Warning	1012	Raising a warning
Error	55555	RESIGNAL to error
drop procedure test_resignal $$
create procedure test_resignal()
begin
DECLARE not_found CONDITION FOR SQLSTATE "02ABC";
DECLARE CONTINUE HANDLER for 1012
begin
select "handler called";
RESIGNAL SQLSTATE "01222" SET
MESSAGE_TEXT = "RESIGNAL to warning",
MYSQL_ERRNO = 55555 ;
end;
SIGNAL not_found SET
MESSAGE_TEXT = "Raising a not found",
MYSQL_ERRNO = 1012;
end $$
call test_resignal() $$
handler called
handler called
Warnings:
Error	1012	Raising a not found
Warning	55555	RESIGNAL to warning
drop procedure test_resignal $$
create procedure test_resignal()
begin
DECLARE not_found CONDITION FOR SQLSTATE "02ABC";
DECLARE CONTINUE HANDLER for 1012
begin
select "before RESIGNAL";
RESIGNAL SQLSTATE "02222" SET
MESSAGE_TEXT = "RESIGNAL to not found",
MYSQL_ERRNO = 55555 ;
select "after RESIGNAL";
end;
SIGNAL not_found SET
MESSAGE_TEXT = "Raising a not found",
MYSQL_ERRNO = 1012;
end $$
call test_resignal() $$
before RESIGNAL
before RESIGNAL
ERROR 02222: RESIGNAL to not found
show warnings $$
Level	Code	Message
Error	1012	Raising a not found
Error	55555	RESIGNAL to not found
drop procedure test_resignal $$
create procedure test_resignal()
begin
DECLARE not_found CONDITION FOR SQLSTATE "02ABC";
DECLARE CONTINUE HANDLER for 1012
begin
select "before RESIGNAL";
RESIGNAL SQLSTATE "33333" SET
MESSAGE_TEXT = "RESIGNAL to error",
MYSQL_ERRNO = 55555 ;
select "after RESIGNAL";
end;
SIGNAL not_found SET
MESSAGE_TEXT = "Raising a not found",
MYSQL_ERRNO = 1012;
end $$
call test_resignal() $$
before RESIGNAL
before RESIGNAL
ERROR 33333: RESIGNAL to error
show warnings $$
Level	Code	Message
Error	1012	Raising a not found
Error	55555	RESIGNAL to error
drop procedure test_resignal $$
create procedure test_resignal()
begin
DECLARE error CONDITION FOR SQLSTATE "AAAAA";
DECLARE CONTINUE HANDLER for 1012
begin
select "handler called";
RESIGNAL SQLSTATE "01222" SET
MESSAGE_TEXT = "RESIGNAL to warning",
MYSQL_ERRNO = 55555 ;
end;
SIGNAL error SET
MESSAGE_TEXT = "Raising an error",
MYSQL_ERRNO = 1012;
end $$
call test_resignal() $$
handler called
handler called
Warnings:
Error	1012	Raising an error
Warning	55555	RESIGNAL to warning
drop procedure test_resignal $$
create procedure test_resignal()
begin
DECLARE error CONDITION FOR SQLSTATE "AAAAA";
DECLARE CONTINUE HANDLER for 1012
begin
select "before RESIGNAL";
RESIGNAL SQLSTATE "02222" SET
MESSAGE_TEXT = "RESIGNAL to not found",
MYSQL_ERRNO = 55555 ;
select "after RESIGNAL";
end;
SIGNAL error SET
MESSAGE_TEXT = "Raising an error",
MYSQL_ERRNO = 1012;
end $$
call test_resignal() $$
before RESIGNAL
before RESIGNAL
ERROR 02222: RESIGNAL to not found
show warnings $$
Level	Code	Message
Error	1012	Raising an error
Error	55555	RESIGNAL to not found
drop procedure test_resignal $$
create procedure test_resignal()
begin
DECLARE error CONDITION FOR SQLSTATE "AAAAA";
DECLARE CONTINUE HANDLER for 1012
begin
select "before RESIGNAL";
RESIGNAL SQLSTATE "33333" SET
MESSAGE_TEXT = "RESIGNAL to error",
MYSQL_ERRNO = 55555 ;
select "after RESIGNAL";
end;
SIGNAL error SET
MESSAGE_TEXT = "Raising an error",
MYSQL_ERRNO = 1012;
end $$
call test_resignal() $$
before RESIGNAL
before RESIGNAL
ERROR 33333: RESIGNAL to error
show warnings $$
Level	Code	Message
Error	1012	Raising an error
Error	55555	RESIGNAL to error
drop procedure test_resignal $$
SET sql_mode = 'NO_ENGINE_SUBSTITUTION';
create procedure test_resignal()
begin
DECLARE CONTINUE HANDLER for sqlwarning
begin
select "handler called";
RESIGNAL SQLSTATE "01111" SET
MESSAGE_TEXT = "RESIGNAL to a warning",
MYSQL_ERRNO = 55555 ;
end;
insert into t_warn set a= 9999999999999999;
end $$
call test_resignal() $$
handler called
handler called
Warnings:
Warning	1264	Out of range value for column 'a' at row 1
Warning	55555	RESIGNAL to a warning
drop procedure test_resignal $$
create procedure test_resignal()
begin
DECLARE CONTINUE HANDLER for sqlwarning
begin
select "before RESIGNAL";
RESIGNAL SQLSTATE "02444" SET
MESSAGE_TEXT = "RESIGNAL to a not found",
MYSQL_ERRNO = 55555 ;
select "after RESIGNAL";
end;
insert into t_warn set a= 9999999999999999;
end $$
call test_resignal() $$
before RESIGNAL
before RESIGNAL
ERROR 02444: RESIGNAL to a not found
show warnings $$
Level	Code	Message
Warning	1264	Out of range value for column 'a' at row 1
Error	55555	RESIGNAL to a not found
drop procedure test_resignal $$
create procedure test_resignal()
begin
DECLARE CONTINUE HANDLER for sqlwarning
begin
select "before RESIGNAL";
RESIGNAL SQLSTATE "44444" SET
MESSAGE_TEXT = "RESIGNAL to an error",
MYSQL_ERRNO = 55555 ;
select "after RESIGNAL";
end;
insert into t_warn set a= 9999999999999999;
end $$
call test_resignal() $$
before RESIGNAL
before RESIGNAL
ERROR 44444: RESIGNAL to an error
show warnings $$
Level	Code	Message
Warning	1264	Out of range value for column 'a' at row 1
Error	55555	RESIGNAL to an error
drop procedure test_resignal $$
SET sql_mode = default;
# 3 tests:
# {not found}
# --> RESIGNAL SQLSTATE {warning, not found, error} SET ...
create procedure test_resignal()
begin
DECLARE x integer;
DECLARE c cursor for select * from t_cursor;
DECLARE CONTINUE HANDLER for not found
begin
select "handler called";
RESIGNAL SQLSTATE "01111" SET
MESSAGE_TEXT = "RESIGNAL to a warning",
MYSQL_ERRNO = 55555 ;
end;
open c;
fetch c into x;
# We don't explicitly close for once, but rely on the end-of-scope clean-up
# because we don't want for CLOSE to reset the diagnostics area ...
# close c;
end $$
call test_resignal() $$
handler called
handler called
Warnings:
Error	1329	No data - zero rows fetched, selected, or processed
Warning	55555	RESIGNAL to a warning
drop procedure test_resignal $$
create procedure test_resignal()
begin
DECLARE x integer;
DECLARE c cursor for select * from t_cursor;
DECLARE CONTINUE HANDLER for not found
begin
select "before RESIGNAL";
RESIGNAL SQLSTATE "02444" SET
MESSAGE_TEXT = "RESIGNAL to a not found",
MYSQL_ERRNO = 55555 ;
select "after RESIGNAL";
end;
open c;
fetch c into x;
close c;
end $$
call test_resignal() $$
before RESIGNAL
before RESIGNAL
ERROR 02444: RESIGNAL to a not found
show warnings $$
Level	Code	Message
Error	1329	No data - zero rows fetched, selected, or processed
Error	55555	RESIGNAL to a not found
drop procedure test_resignal $$
create procedure test_resignal()
begin
DECLARE x integer;
DECLARE c cursor for select * from t_cursor;
DECLARE CONTINUE HANDLER for not found
begin
select "before RESIGNAL";
RESIGNAL SQLSTATE "44444" SET
MESSAGE_TEXT = "RESIGNAL to an error",
MYSQL_ERRNO = 55555 ;
select "after RESIGNAL";
end;
open c;
fetch c into x;
close c;
end $$
call test_resignal() $$
before RESIGNAL
before RESIGNAL
ERROR 44444: RESIGNAL to an error
show warnings $$
Level	Code	Message
Error	1329	No data - zero rows fetched, selected, or processed
Error	55555	RESIGNAL to an error
drop procedure test_resignal $$
create procedure test_resignal()
begin
DECLARE CONTINUE HANDLER for sqlexception
begin
select "handler called";
RESIGNAL SQLSTATE "01111" SET
MESSAGE_TEXT = "RESIGNAL to a warning",
MYSQL_ERRNO = 55555 ;
end;
drop table no_such_table;
end $$
call test_resignal() $$
handler called
handler called
Warnings:
Error	1051	Unknown table 'test.no_such_table'
Warning	55555	RESIGNAL to a warning
drop procedure test_resignal $$
create procedure test_resignal()
begin
DECLARE CONTINUE HANDLER for sqlexception
begin
select "before RESIGNAL";
RESIGNAL SQLSTATE "02444" SET
MESSAGE_TEXT = "RESIGNAL to a not found",
MYSQL_ERRNO = 55555 ;
select "after RESIGNAL";
end;
drop table no_such_table;
end $$
call test_resignal() $$
before RESIGNAL
before RESIGNAL
ERROR 02444: RESIGNAL to a not found
show warnings $$
Level	Code	Message
Error	1051	Unknown table 'test.no_such_table'
Error	55555	RESIGNAL to a not found
drop procedure test_resignal $$
create procedure test_resignal()
begin
DECLARE CONTINUE HANDLER for sqlexception
begin
select "before RESIGNAL";
RESIGNAL SQLSTATE "44444" SET
MESSAGE_TEXT = "RESIGNAL to an error",
MYSQL_ERRNO = 55555 ;
select "after RESIGNAL";
end;
drop table no_such_table;
end $$
call test_resignal() $$
before RESIGNAL
before RESIGNAL
ERROR 44444: RESIGNAL to an error
show warnings $$
Level	Code	Message
Error	1051	Unknown table 'test.no_such_table'
Error	55555	RESIGNAL to an error
drop procedure test_resignal $$
#
# More complex cases
#
drop procedure if exists peter_p1 $$
drop procedure if exists peter_p2 $$
CREATE PROCEDURE peter_p1 ()
BEGIN
DECLARE x CONDITION FOR 1231;
DECLARE EXIT HANDLER FOR x
BEGIN
SELECT '2';
RESIGNAL SET MYSQL_ERRNO = 9999;
END;
BEGIN
DECLARE EXIT HANDLER FOR x
BEGIN
SELECT '1';
RESIGNAL SET SCHEMA_NAME = 'test';
END;
SET @@sql_mode=NULL;
END;
END
$$
CREATE PROCEDURE peter_p2 ()
BEGIN
DECLARE x CONDITION for 9999;
DECLARE EXIT HANDLER FOR x
BEGIN
SELECT '3';
RESIGNAL SET MESSAGE_TEXT = 'Hi, I am a useless error message';
END;
CALL peter_p1();
END
$$
CALL peter_p2() $$
1
1
2
2
3
3
ERROR 42000: Hi, I am a useless error message
show warnings $$
Level	Code	Message
Error	9999	Hi, I am a useless error message
drop procedure peter_p1 $$
drop procedure peter_p2 $$
CREATE PROCEDURE peter_p1 ()
BEGIN
DECLARE x CONDITION FOR SQLSTATE '42000';
DECLARE EXIT HANDLER FOR x
BEGIN
SHOW WARNINGS;
RESIGNAL x SET MYSQL_ERRNO = 9999;
END;
BEGIN
DECLARE EXIT HANDLER FOR x
BEGIN
SHOW WARNINGS;
RESIGNAL x SET
SCHEMA_NAME = 'test',
MYSQL_ERRNO= 1232;
END;
/* Raises ER_WRONG_VALUE_FOR_VAR : 1231, SQLSTATE 42000 */
SET @@sql_mode=NULL;
END;
END
$$
CREATE PROCEDURE peter_p2 ()
BEGIN
DECLARE x CONDITION for SQLSTATE '42000';
DECLARE EXIT HANDLER FOR x
BEGIN
SHOW WARNINGS;
RESIGNAL x SET
MESSAGE_TEXT = 'Hi, I am a useless error message',
MYSQL_ERRNO = 9999;
END;
CALL peter_p1();
END
$$
CALL peter_p2() $$
Level	Code	Message
Error	1231	Variable 'sql_mode' can't be set to the value of 'NULL'
Level	Code	Message
Error	1231	Variable 'sql_mode' can't be set to the value of 'NULL'
Error	1232	Variable 'sql_mode' can't be set to the value of 'NULL'
Level	Code	Message
Error	1231	Variable 'sql_mode' can't be set to the value of 'NULL'
Error	1232	Variable 'sql_mode' can't be set to the value of 'NULL'
Error	9999	Variable 'sql_mode' can't be set to the value of 'NULL'
ERROR 42000: Hi, I am a useless error message
show warnings $$
Level	Code	Message
Error	1231	Variable 'sql_mode' can't be set to the value of 'NULL'
Error	1232	Variable 'sql_mode' can't be set to the value of 'NULL'
Error	9999	Variable 'sql_mode' can't be set to the value of 'NULL'
Error	9999	Hi, I am a useless error message
drop procedure peter_p1 $$
drop procedure peter_p2 $$
drop procedure if exists peter_p3 $$
Warnings:
Note	1305	PROCEDURE test.peter_p3 does not exist
create procedure peter_p3()
begin
declare continue handler for sqlexception
resignal sqlstate '99002' set mysql_errno = 2;
signal sqlstate '99001' set mysql_errno = 1, message_text = "Original";
end $$
call peter_p3() $$
ERROR 99002: Original
show warnings $$
Level	Code	Message
Error	1	Original
Error	2	Original
drop procedure peter_p3 $$
drop table t_warn;
drop table t_cursor;
#
# Miscelaneous test cases
#
create procedure test_signal()
begin
DECLARE foo CONDITION FOR SQLSTATE '12345';
SIGNAL foo SET MYSQL_ERRNO = 0x12; /* 18 */
end $$
call test_signal $$
ERROR 12345: Unhandled user-defined exception condition
drop procedure test_signal $$
create procedure test_signal()
begin
DECLARE foo CONDITION FOR SQLSTATE '12345';
SIGNAL foo SET MYSQL_ERRNO = 0b00010010; /* 18 */
end $$
call test_signal $$
ERROR 12345: Unhandled user-defined exception condition
drop procedure test_signal $$
create procedure test_signal()
begin
DECLARE foo CONDITION FOR SQLSTATE '12345';
SIGNAL foo SET MYSQL_ERRNO = '65'; /* 65 */
end $$
call test_signal $$
ERROR 12345: Unhandled user-defined exception condition
drop procedure test_signal $$
create procedure test_signal()
begin
DECLARE foo CONDITION FOR SQLSTATE '12345';
SIGNAL foo SET MYSQL_ERRNO = 'A'; /* illegal */
end $$
call test_signal $$
ERROR 42000: Variable 'MYSQL_ERRNO' can't be set to the value of 'A'
drop procedure test_signal $$
create procedure test_signal()
begin
DECLARE foo CONDITION FOR SQLSTATE '12345';
SIGNAL foo SET MYSQL_ERRNO = "65"; /* 65 */
end $$
call test_signal $$
ERROR 12345: Unhandled user-defined exception condition
drop procedure test_signal $$
create procedure test_signal()
begin
DECLARE foo CONDITION FOR SQLSTATE '12345';
SIGNAL foo SET MYSQL_ERRNO = "A"; /* illegal */
end $$
call test_signal $$
ERROR 42000: Variable 'MYSQL_ERRNO' can't be set to the value of 'A'
drop procedure test_signal $$
create procedure test_signal()
begin
DECLARE foo CONDITION FOR SQLSTATE '12345';
SIGNAL foo SET MYSQL_ERRNO = `65`; /* illegal */
end $$
call test_signal $$
ERROR 42S22: Unknown column '65' in 'field list'
drop procedure test_signal $$
create procedure test_signal()
begin
DECLARE foo CONDITION FOR SQLSTATE '12345';
SIGNAL foo SET MYSQL_ERRNO = `A`; /* illegal */
end $$
call test_signal $$
ERROR 42S22: Unknown column 'A' in 'field list'
drop procedure test_signal $$
create procedure test_signal()
begin
DECLARE foo CONDITION FOR SQLSTATE '12345';
SIGNAL foo SET MYSQL_ERRNO = 3.141592; /* 3 */
end $$
call test_signal $$
ERROR 12345: Unhandled user-defined exception condition
drop procedure test_signal $$
create procedure test_signal()
begin
DECLARE foo CONDITION FOR SQLSTATE '12345';
SIGNAL foo SET MYSQL_ERRNO = 1000,
MESSAGE_TEXT= 0x41; /* A */
end $$
call test_signal $$
ERROR 12345: A
drop procedure test_signal $$
create procedure test_signal()
begin
DECLARE foo CONDITION FOR SQLSTATE '12345';
SIGNAL foo SET MYSQL_ERRNO = 1000,
MESSAGE_TEXT= 0b01000001; /* A */
end $$
call test_signal $$
ERROR 12345: A
drop procedure test_signal $$
create procedure test_signal()
begin
DECLARE foo CONDITION FOR SQLSTATE '12345';
SIGNAL foo SET MYSQL_ERRNO = 1000,
MESSAGE_TEXT = "Hello";
end $$
call test_signal $$
ERROR 12345: Hello
drop procedure test_signal $$
create procedure test_signal()
begin
DECLARE foo CONDITION FOR SQLSTATE '12345';
SIGNAL foo SET MYSQL_ERRNO = 1000,
MESSAGE_TEXT = 'Hello';
end $$
call test_signal $$
ERROR 12345: Hello
drop procedure test_signal $$
create procedure test_signal()
begin
DECLARE foo CONDITION FOR SQLSTATE '12345';
SIGNAL foo SET MYSQL_ERRNO = 1000,
MESSAGE_TEXT = `Hello`;
end $$
call test_signal $$
ERROR 42S22: Unknown column 'Hello' in 'field list'
drop procedure test_signal $$
create procedure test_signal()
begin
DECLARE foo CONDITION FOR SQLSTATE '12345';
SIGNAL foo SET MYSQL_ERRNO = 1000,
MESSAGE_TEXT = 65.4321;
end $$
call test_signal $$
ERROR 12345: 65.4321
drop procedure test_signal $$
create procedure test_signal()
begin
DECLARE céèçà foo CONDITION FOR SQLSTATE '12345';
SIGNAL céèçà SET MYSQL_ERRNO = 1000;
end $$
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'foo CONDITION FOR SQLSTATE '12345';
SIGNAL céèçà SET MYSQL_ERRNO = 1000;
end' at line 3
create procedure test_signal()
begin
DECLARE "céèçà" CONDITION FOR SQLSTATE '12345';
SIGNAL "céèçà" SET MYSQL_ERRNO = 1000;
end $$
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near '"céèçà" CONDITION FOR SQLSTATE '12345';
SIGNAL "céèçà" SET MYSQL_ERRNO =' at line 3
create procedure test_signal()
begin
DECLARE 'céèçà' CONDITION FOR SQLSTATE '12345';
SIGNAL 'céèçà' SET MYSQL_ERRNO = 1000;
end $$
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near ''céèçà' CONDITION FOR SQLSTATE '12345';
SIGNAL 'céèçà' SET MYSQL_ERRNO =' at line 3
create procedure test_signal()
begin
DECLARE `céèçà` CONDITION FOR SQLSTATE '12345';
SIGNAL `céèçà` SET MYSQL_ERRNO = 1000;
end $$
call test_signal $$
ERROR 12345: Unhandled user-defined exception condition
drop procedure test_signal $$
create procedure test_signal()
begin
SIGNAL SQLSTATE '77777' SET MYSQL_ERRNO = 1000, MESSAGE_TEXT='ÁÂÃÅÄ';
end $$
drop procedure test_signal $$
#
# WL#6406 Stacked diagnostic areas
#
# Double RESIGNAL of warning
#
SET sql_mode = 'NO_ENGINE_SUBSTITUTION';
CREATE PROCEDURE p1()
BEGIN
DECLARE CONTINUE HANDLER FOR SQLWARNING
BEGIN
RESIGNAL;
RESIGNAL;
END;
SELECT 10 + 'a';
END $$
CALL p1();
10 + 'a'
10
Warnings:
Warning	1292	Truncated incorrect DOUBLE value: 'a'
DROP PROCEDURE p1;
CREATE PROCEDURE p1()
BEGIN
DECLARE CONTINUE HANDLER FOR SQLWARNING
BEGIN
RESIGNAL SET MESSAGE_TEXT= '1st resignal';
RESIGNAL SET MESSAGE_TEXT= '2nd resignal';
END;
SELECT 10 + 'a';
END $$
CALL p1();
10 + 'a'
10
Warnings:
Warning	1292	2nd resignal
DROP PROCEDURE p1;
CREATE PROCEDURE p1()
BEGIN
DECLARE CONTINUE HANDLER FOR SQLWARNING
BEGIN
RESIGNAL SQLSTATE '01000' SET MESSAGE_TEXT= '1st resignal';
RESIGNAL SQLSTATE '01000' SET MESSAGE_TEXT= '2nd resignal';
END;
SELECT 10 + 'a';
END $$
CALL p1();
10 + 'a'
10
Warnings:
Warning	1292	Truncated incorrect DOUBLE value: 'a'
Warning	1642	1st resignal
Warning	1642	2nd resignal
DROP PROCEDURE p1;
# RESIGNAL warning + RESIGNAL error
#
CREATE PROCEDURE p1()
BEGIN
DECLARE CONTINUE HANDLER FOR SQLWARNING
BEGIN
RESIGNAL SQLSTATE '01000' SET MESSAGE_TEXT= '1st resignal';
RESIGNAL SQLSTATE 'HY000' SET MESSAGE_TEXT= '2nd resignal';
END;
SELECT 10 + 'a';
END $$
CALL p1();
10 + 'a'
10
ERROR HY000: 2nd resignal
SHOW WARNINGS;
Level	Code	Message
Warning	1292	Truncated incorrect DOUBLE value: 'a'
Warning	1642	1st resignal
Error	1644	2nd resignal
DROP PROCEDURE p1;
# Unhandled errors raised in nested handlers
#
CREATE PROCEDURE p1()
BEGIN
DECLARE CONTINUE HANDLER FOR 1051
BEGIN
DROP DATABASE none;  # Error 1008
END;
DROP TABLE none;  # Error 1051
END $$
CALL p1();
ERROR HY000: Can't drop database 'none'; database doesn't exist
SHOW WARNINGS;
Level	Code	Message
Error	1008	Can't drop database 'none'; database doesn't exist
DROP PROCEDURE p1;
# RESIGNAL of warning which fails
#
CREATE PROCEDURE p1()
BEGIN
DECLARE CONTINUE HANDLER FOR SQLWARNING RESIGNAL SET MESSAGE_TEXT = asdf;
SELECT 10 + 'a';
END $$
CREATE PROCEDURE p2()
BEGIN
DECLARE CONTINUE HANDLER FOR SQLEXCEPTION SELECT 'handled' as Msg;
BEGIN
DECLARE CONTINUE HANDLER FOR SQLWARNING RESIGNAL SET MESSAGE_TEXT = asdf;
SELECT 10 + 'a';
END;
END $$
CREATE PROCEDURE p3()
BEGIN
DECLARE EXIT HANDLER FOR SQLEXCEPTION SELECT 'handled' as Msg;
BEGIN
DECLARE CONTINUE HANDLER FOR SQLWARNING RESIGNAL SET MESSAGE_TEXT = asdf;
SELECT 10 + 'a';
END;
END $$
CREATE PROCEDURE p4()
BEGIN
DECLARE CONTINUE HANDLER FOR SQLWARNING
RESIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = asdf;
SELECT 10 + 'a';
END $$
CREATE PROCEDURE p5()
BEGIN
DECLARE CONTINUE HANDLER FOR SQLEXCEPTION SELECT 'handled' as Msg;
BEGIN
DECLARE CONTINUE HANDLER FOR SQLWARNING
RESIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = asdf;
SELECT 10 + 'a';
END;
END $$
CREATE PROCEDURE p6()
BEGIN
DECLARE EXIT HANDLER FOR SQLEXCEPTION SELECT 'handled' as Msg;
BEGIN
DECLARE CONTINUE HANDLER FOR SQLWARNING
RESIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = asdf;
SELECT 10 + 'a';
END;
END $$
CALL p1();
10 + 'a'
10
ERROR 42S22: Unknown column 'asdf' in 'field list'
CALL p2();
10 + 'a'
10
Msg
handled
CALL p3();
10 + 'a'
10
Msg
handled
CALL p4();
10 + 'a'
10
ERROR 42S22: Unknown column 'asdf' in 'field list'
CALL p5();
10 + 'a'
10
Msg
handled
CALL p6();
10 + 'a'
10
Msg
handled
DROP PROCEDURE p1;
DROP PROCEDURE p2;
DROP PROCEDURE p3;
DROP PROCEDURE p4;
DROP PROCEDURE p5;
DROP PROCEDURE p6;
# RESIGNAL of error which fails
#
CREATE PROCEDURE p1()
BEGIN
DECLARE CONTINUE HANDLER FOR SQLEXCEPTION RESIGNAL SET MESSAGE_TEXT = asdf;
DROP TABLE none;
END $$
CREATE PROCEDURE p2()
BEGIN
DECLARE CONTINUE HANDLER FOR SQLEXCEPTION SELECT 'handled' as Msg;
BEGIN
DECLARE CONTINUE HANDLER FOR SQLEXCEPTION RESIGNAL SET MESSAGE_TEXT = asdf;
DROP TABLE none;
END;
END $$
CREATE PROCEDURE p3()
BEGIN
DECLARE EXIT HANDLER FOR SQLEXCEPTION SELECT 'handled' as Msg;
BEGIN
DECLARE CONTINUE HANDLER FOR SQLEXCEPTION RESIGNAL SET MESSAGE_TEXT = asdf;
DROP TABLE none;
END;
END $$
CREATE PROCEDURE p4()
BEGIN
DECLARE CONTINUE HANDLER FOR SQLEXCEPTION
RESIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = asdf;
DROP TABLE none;
END $$
CREATE PROCEDURE p5()
BEGIN
DECLARE CONTINUE HANDLER FOR SQLEXCEPTION SELECT 'handled' as Msg;
BEGIN
DECLARE CONTINUE HANDLER FOR SQLEXCEPTION
RESIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = asdf;
DROP TABLE none;
END;
END $$
CREATE PROCEDURE p6()
BEGIN
DECLARE EXIT HANDLER FOR SQLEXCEPTION SELECT 'handled' as Msg;
BEGIN
DECLARE CONTINUE HANDLER FOR SQLEXCEPTION
RESIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = asdf;
DROP TABLE none;
END;
END $$
CALL p1();
ERROR 42S22: Unknown column 'asdf' in 'field list'
CALL p2();
Msg
handled
CALL p3();
Msg
handled
CALL p4();
ERROR 42S22: Unknown column 'asdf' in 'field list'
CALL p5();
Msg
handled
CALL p6();
Msg
handled
DROP PROCEDURE p1;
DROP PROCEDURE p2;
DROP PROCEDURE p3;
DROP PROCEDURE p4;
DROP PROCEDURE p5;
DROP PROCEDURE p6;
# Error condition originating from a DA which has been destroyed
# when the condition is handled.
#
CREATE FUNCTION f1() RETURNS INTEGER
BEGIN
DECLARE EXIT HANDLER FOR SQLEXCEPTION RESIGNAL;
INSERT INTO none VALUES (NULL);
RETURN 1;
END|
CREATE PROCEDURE p1()
BEGIN
DECLARE EXIT HANDLER FOR SQLEXCEPTION RESIGNAL;
SELECT f1();
END|
CALL p1();
f1()
ERROR 42S02: Table 'test.none' doesn't exist
DROP PROCEDURE p1;
DROP FUNCTION f1;
# RESIGNAL with max_error_count = 0
#
CREATE PROCEDURE p1()
BEGIN
DECLARE CONTINUE HANDLER FOR SQLEXCEPTION RESIGNAL;
SIGNAL SQLSTATE 'HY000';
END|
CREATE PROCEDURE p2()
BEGIN
DECLARE CONTINUE HANDLER FOR SQLWARNING RESIGNAL;
SIGNAL SQLSTATE '01000';
END|
CREATE PROCEDURE p3()
BEGIN
DECLARE CONTINUE HANDLER FOR SQLEXCEPTION
RESIGNAL SET MESSAGE_TEXT= 'custom error msg';
SIGNAL SQLSTATE 'HY000';
END|
CREATE PROCEDURE p4()
BEGIN
DECLARE CONTINUE HANDLER FOR SQLWARNING
RESIGNAL SET MESSAGE_TEXT= 'custom warning msg';
SIGNAL SQLSTATE '01000';
END|
CREATE PROCEDURE p5()
BEGIN
DECLARE CONTINUE HANDLER FOR SQLEXCEPTION
RESIGNAL SQLSTATE '45000' SET MESSAGE_TEXT= 'custom error msg';
SIGNAL SQLSTATE 'HY000';
END|
CREATE PROCEDURE p6()
BEGIN
DECLARE CONTINUE HANDLER FOR SQLWARNING
RESIGNAL SQLSTATE '45000' SET MESSAGE_TEXT= 'custom warning msg';
SIGNAL SQLSTATE '01000';
END|
CREATE PROCEDURE p7()
BEGIN
DECLARE CONTINUE HANDLER FOR SQLEXCEPTION SELECT 'handled' AS Msg;
BEGIN
DECLARE CONTINUE HANDLER FOR SQLEXCEPTION RESIGNAL;
SIGNAL SQLSTATE 'HY000';
END;
END|
CREATE PROCEDURE p8()
BEGIN
DECLARE CONTINUE HANDLER FOR SQLWARNING SELECT 'handled' AS Msg;
BEGIN
DECLARE CONTINUE HANDLER FOR SQLWARNING RESIGNAL;
SIGNAL SQLSTATE '01000';
END;
END|
CREATE PROCEDURE p9()
BEGIN
DECLARE CONTINUE HANDLER FOR SQLEXCEPTION SELECT 'handled' AS Msg;
BEGIN
DECLARE CONTINUE HANDLER FOR SQLEXCEPTION
RESIGNAL SET MESSAGE_TEXT= 'custom error msg';
SIGNAL SQLSTATE 'HY000';
END;
END|
CREATE PROCEDURE p10()
BEGIN
DECLARE CONTINUE HANDLER FOR SQLWARNING SELECT 'handled' AS Msg;
BEGIN
DECLARE CONTINUE HANDLER FOR SQLWARNING
RESIGNAL SET MESSAGE_TEXT= 'custom warning msg';
SIGNAL SQLSTATE '01000';
END;
END|
CREATE PROCEDURE p11()
BEGIN
DECLARE CONTINUE HANDLER FOR SQLEXCEPTION SELECT 'handled' AS Msg;
BEGIN
DECLARE CONTINUE HANDLER FOR SQLEXCEPTION
RESIGNAL SQLSTATE '45000' SET MESSAGE_TEXT= 'custom error msg';
SIGNAL SQLSTATE 'HY000';
END;
END|
CREATE PROCEDURE p12()
BEGIN
DECLARE CONTINUE HANDLER FOR SQLEXCEPTION SELECT 'handled' AS Msg;
BEGIN
DECLARE CONTINUE HANDLER FOR SQLWARNING
RESIGNAL SQLSTATE '45000' SET MESSAGE_TEXT= 'custom warning msg';
SIGNAL SQLSTATE '01000';
END;
END|
SET max_error_count= 0;
CALL p1();
ERROR HY000: Unhandled user-defined exception condition
CALL p2();
CALL p3();
ERROR HY000: custom error msg
CALL p4();
CALL p5();
ERROR 45000: custom error msg
CALL p6();
CALL p7();
Msg
handled
CALL p8();
CALL p9();
Msg
handled
CALL p10();
CALL p11();
Msg
handled
CALL p12();
SET max_error_count= DEFAULT;
DROP PROCEDURE p1;
DROP PROCEDURE p2;
DROP PROCEDURE p3;
DROP PROCEDURE p4;
DROP PROCEDURE p5;
DROP PROCEDURE p6;
DROP PROCEDURE p7;
DROP PROCEDURE p8;
DROP PROCEDURE p9;
DROP PROCEDURE p10;
DROP PROCEDURE p11;
DROP PROCEDURE p12;
SET sql_mode = default;
